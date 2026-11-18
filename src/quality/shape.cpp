// Owner: Ranju Kim
// Shape (weight 25%): does the form match the target pattern?
//   1. overlap: scale and shift the user pattern to the reference's area and
//      centroid, overlay the two masks and take IoU (proposal: "align the
//      size and position of the two patterns, overlay them")
//   2. symmetry: IoU of the pattern with its mirror image about its own axis
//      (best line through the centroid within +-20 deg of vertical, see
//      findSymmetryAxis); only less symmetry than the reference is penalized.
//      For a pattern with several axes, call mirrorIoU once per axis part.
//   3. Hu moment distance (Hu 1962): size/position/rotation-invariant shape
//      descriptors, compared on a log scale
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "quality/quality.h"

namespace la::quality {

namespace {

// Draft constants, re-check with real photos.
constexpr float kSymmetrySigma = 0.10f;
constexpr float kHuSigma = 0.5f;
constexpr int kHuUsed = 4;   // invariants 5-7 are tiny and noisy for blob shapes
constexpr float kAxisSearchDeg = 20.f;  // mirror line searched within +-20 deg of vertical
constexpr float kOk = 0.85f;

struct Moments {
    double area = 0.0, cx = 0.0, cy = 0.0;
};

Moments centroidOf(const cv::Mat& mask) {
    Moments m;
    for (int y = 0; y < mask.rows; ++y) {
        const uchar* p = mask.ptr<uchar>(y);
        for (int x = 0; x < mask.cols; ++x)
            if (p[x]) { m.area += 1.0; m.cx += x + 0.5; m.cy += y + 0.5; }
    }
    if (m.area > 0) { m.cx /= m.area; m.cy /= m.area; }
    return m;
}

bool at(const cv::Mat& mask, int x, int y) {
    return x >= 0 && y >= 0 && x < mask.cols && y < mask.rows && mask.ptr<uchar>(y)[x] != 0;
}

double iou(const cv::Mat& a, const cv::Mat& b) {
    double inter = 0.0, uni = 0.0;
    for (int y = 0; y < a.rows; ++y)
        for (int x = 0; x < a.cols; ++x) {
            const bool pa = a.ptr<uchar>(y)[x] != 0, pb = b.ptr<uchar>(y)[x] != 0;
            inter += pa && pb;
            uni += pa || pb;
        }
    return uni > 0 ? inter / uni : 0.0;
}

// Symmetry about the pattern's own axis: the best mirror line within
// +-kAxisSearchDeg of vertical (the axis normalization puts upright). A
// pattern that is symmetric but a few degrees tilted keeps full symmetry;
// the tilt itself is judged by the position item.
float symmetry(const cv::Mat& mask) {
    return mirrorIoU(mask, findSymmetryAxis(mask, 0.f, kAxisSearchDeg));
}

// User mask scaled (by sqrt of the area ratio) and shifted onto the reference
// centroid, then compared with the reference mask.
float alignedIoU(const cv::Mat& user, const cv::Mat& ref) {
    const Moments mu = centroidOf(user), mr = centroidOf(ref);
    if (mu.area <= 0 || mr.area <= 0) return 0.f;
    const double s = std::sqrt(mr.area / mu.area);  // user -> reference scale
    cv::Mat moved(ref.rows, ref.cols, CV_8UC1);
    for (int y = 0; y < ref.rows; ++y)
        for (int x = 0; x < ref.cols; ++x) {
            const double ux = (x + 0.5 - mr.cx) / s + mu.cx;  // inverse mapping, nearest pixel
            const double uy = (y + 0.5 - mr.cy) / s + mu.cy;
            moved.ptr<uchar>(y)[x] =
                at(user, int(std::floor(ux)), int(std::floor(uy))) ? 255 : 0;
        }
    return static_cast<float>(iou(moved, ref));
}

// First kHuUsed Hu invariants on a log10 scale.
std::array<double, kHuUsed> logHu(const cv::Mat& mask) {
    const Moments m = centroidOf(mask);
    double mu[4][4] = {};  // central moments mu_pq, p + q <= 3
    for (int y = 0; y < mask.rows; ++y) {
        const uchar* p = mask.ptr<uchar>(y);
        for (int x = 0; x < mask.cols; ++x) {
            if (!p[x]) continue;
            const double dx = x + 0.5 - m.cx, dy = y + 0.5 - m.cy;
            double xp = 1.0;
            for (int i = 0; i <= 3; ++i) {
                double yq = 1.0;
                for (int j = 0; i + j <= 3; ++j) {
                    mu[i][j] += xp * yq;
                    yq *= dy;
                }
                xp *= dx;
            }
        }
    }
    auto eta = [&](int p, int q) { return mu[p][q] / std::pow(m.area, 1.0 + (p + q) / 2.0); };
    const double n20 = eta(2, 0), n02 = eta(0, 2), n11 = eta(1, 1);
    const double n30 = eta(3, 0), n03 = eta(0, 3), n21 = eta(2, 1), n12 = eta(1, 2);

    const double h[4] = {
        n20 + n02,
        (n20 - n02) * (n20 - n02) + 4 * n11 * n11,
        (n30 - 3 * n12) * (n30 - 3 * n12) + (3 * n21 - n03) * (3 * n21 - n03),
        (n30 + n12) * (n30 + n12) + (n21 + n03) * (n21 + n03),
    };
    // h1..h4 are sums of squares, so never negative. For symmetric shapes
    // h2..h4 are 0 in theory but tiny pixel-noise values (1e-9, 1e-11, ...)
    // in practice, and their logs would differ wildly between two almost
    // identical shapes. Values below these floors are treated as "zero".
    constexpr double kFloor[kHuUsed] = {1e-3, 1e-5, 1e-6, 1e-6};
    std::array<double, kHuUsed> out{};
    for (int i = 0; i < kHuUsed; ++i) out[i] = std::log10(std::max(h[i], kFloor[i]));
    return out;
}

float huDistance(const cv::Mat& a, const cv::Mat& b) {
    const auto ha = logHu(a), hb = logHu(b);
    double d = 0.0;
    for (int i = 0; i < kHuUsed; ++i) d += std::abs(ha[i] - hb[i]);
    return static_cast<float>(d / kHuUsed);
}

std::string fmt(const char* f, double a, double b) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), f, a, b);
    return buf;
}

}  // namespace

// Note: Hu invariants and mirror symmetry do not change when a pattern is
// rotated or flipped (an upside-down heart scores full marks on both); only
// the aligned overlap (40%) notices it. Normalization keeps the axis upright,
// so this is accepted.
ItemScore evalShape(const PatternMask& userMask, const PatternMask& refMask) {
    ItemScore it;
    // Check the real pixel count too, so an inconsistent PatternMask
    // (area > 0 but no pixels set) cannot cause a division by zero.
    if (userMask.mask.empty() || userMask.area <= 0.f || centroidOf(userMask.mask).area <= 0.0) {
        it.feedback = "No pattern detected.";
        return it;
    }
    if (refMask.mask.empty() || refMask.area <= 0.f || centroidOf(refMask.mask).area <= 0.0) {
        it.feedback = "Reference pattern not detected.";
        return it;
    }

    const float symU = symmetry(userMask.mask), symR = symmetry(refMask.mask);
    const float overlap = alignedIoU(userMask.mask, refMask.mask);
    const float hu = huDistance(userMask.mask, refMask.mask);
    it.raw = symU;
    it.ref = symR;

    const float sSym = notWorse(symU, symR, kSymmetrySigma, true);
    const float sHu = std::exp(-hu / kHuSigma);
    it.score = 0.4f * overlap + 0.3f * sSym + 0.3f * sHu;

    // Feedback goes in order of how actionable it is for a barista (symmetry,
    // then outline, then the abstract Hu distance), not by the lowest
    // sub-score: an asymmetric pattern also raises the Hu distance, and
    // "less symmetric" is the useful message in that case.
    if (sSym < kOk)
        it.feedback = fmt("Pattern is less symmetric than the reference (left-right overlap %.0f%% vs %.0f%%).",
                          100.0 * symU, 100.0 * symR);
    else if (overlap < kOk)
        it.feedback = fmt("Outline differs from the reference: only %.0f%% overlap after aligning size and position.",
                          100.0 * overlap, 0.0);
    else if (sHu < kOk)
        it.feedback = fmt("Overall form differs from the reference (Hu moment distance %.2f).", hu, 0.0);
    else
        it.feedback = "Shape matches the reference.";
    return it;
}

}  // namespace la::quality
