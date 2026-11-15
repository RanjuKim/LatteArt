// Owner: Ranju Kim
// Layer contrast (weight 30%): how sharply the milk foam separates from the
// crema. Three measurements, each compared with the reference photo:
//   1. Michelson contrast (Peli 1990)  (I_foam - I_crema) / (I_foam + I_crema)
//   2. edge width: how many pixels the foam/crema transition is spread over
//   3. high-frequency ratio: share of image energy in fine detail (2D FFT)
// Being better than the reference (more contrast, sharper edges) is never
// penalized.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "basic/fft.h"
#include "basic/filter.h"
#include "basic/morphology.h"
#include "quality/quality.h"

namespace la::quality {

namespace {

// Draft constants, re-check with real photos.
constexpr int kRimMargin = 6;           // same inner cup area as segmentation
constexpr int kCoreMargin = 2;          // skip the blurred boundary when taking means
constexpr float kMichelsonSigma = 0.10f;
constexpr float kRatioSigma = 0.5f;     // for edge-width and HF ratios (user / reference)
constexpr float kMaxEdgeWidth = 50.f;   // px, cap for "no visible edge"
constexpr double kHfCutoff = 0.1;       // cycles per pixel (period shorter than 10 px)
constexpr float kTaper = 16.f;          // px over which the HF window fades to 0 at the edge
constexpr int kEdgeKeepOut = 3;         // px: foam boundary this close to the inner edge is the
                                        // cup edge, not a pattern edge (foam reaching the rim)
constexpr float kOk = 0.85f;            // sub-score above which feedback stays positive

struct ContrastMeasures {
    bool valid = false;
    float foam = 0.f, crema = 0.f;
    float michelson = 0.f;
    float edgeWidth = kMaxEdgeWidth;
    float hfRatio = 0.f;
};

double meanWhere(const cv::Mat& gray, const cv::Mat& sel) {
    double s = 0.0, n = 0.0;
    for (int y = 0; y < gray.rows; ++y) {
        const float* g = gray.ptr<float>(y);
        const uchar* m = sel.ptr<uchar>(y);
        for (int x = 0; x < gray.cols; ++x)
            if (m[x]) { s += g[x]; n += 1.0; }
    }
    return n > 0 ? s / n : -1.0;
}

// Mean of the steepest gradient around each foam boundary pixel. Boundary
// pixels near the inner cup edge are skipped: where the foam runs into the
// rim, the mask edge is only the cut of the cup mask and has no gradient,
// which would make a sharp pattern look blurry.
float boundarySlope(const cv::Mat& gray, const cv::Mat& mask, const cv::Mat& inner) {
    const cv::Mat usable = basic::erode(inner, kEdgeKeepOut);
    const cv::Mat smooth = basic::gaussianBlur(gray, 1.f);
    cv::Mat grad(gray.rows, gray.cols, CV_32FC1);
    for (int y = 0; y < gray.rows; ++y) {
        float* d = grad.ptr<float>(y);
        for (int x = 0; x < gray.cols; ++x) {
            const int xl = std::max(x - 1, 0), xr = std::min(x + 1, gray.cols - 1);
            const int yu = std::max(y - 1, 0), yd = std::min(y + 1, gray.rows - 1);
            const float gx = (smooth.ptr<float>(y)[xr] - smooth.ptr<float>(y)[xl]) / float(xr - xl);
            const float gy = (smooth.ptr<float>(yd)[x] - smooth.ptr<float>(yu)[x]) / float(yd - yu);
            d[x] = std::sqrt(gx * gx + gy * gy);
        }
    }

    double sum = 0.0, n = 0.0;
    for (int y = 1; y < mask.rows - 1; ++y) {
        for (int x = 1; x < mask.cols - 1; ++x) {
            if (!mask.ptr<uchar>(y)[x] || !usable.ptr<uchar>(y)[x]) continue;
            const bool boundary = !mask.ptr<uchar>(y - 1)[x] || !mask.ptr<uchar>(y + 1)[x] ||
                                  !mask.ptr<uchar>(y)[x - 1] || !mask.ptr<uchar>(y)[x + 1];
            if (!boundary) continue;
            float best = 0.f;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const int sx = x + dx, sy = y + dy;
                    if (sx < 0 || sy < 0 || sx >= mask.cols || sy >= mask.rows) continue;
                    best = std::max(best, grad.ptr<float>(sy)[sx]);
                }
            sum += best;
            n += 1.0;
        }
    }
    return n > 0 ? static_cast<float>(sum / n) : 0.f;
}

// Share of spectral energy above kHfCutoff, inside the cup only. The image
// minus its mean is multiplied by a round window that fades smoothly (cosine)
// to 0 over the last kTaper px of the inner cup; a hard cut there would be a
// sharp circular edge whose energy swamps the pattern edges.
float highFrequencyRatio(const cv::Mat& gray, const cv::Mat& inner, const NormalizedCup& cup) {
    const double mean = meanWhere(gray, inner);
    const float rIn = cup.radius - kRimMargin;
    cv::Mat f(gray.rows, gray.cols, CV_32FC1);
    for (int y = 0; y < gray.rows; ++y)
        for (int x = 0; x < gray.cols; ++x) {
            const float dx = x + 0.5f - cup.center.x, dy = y + 0.5f - cup.center.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            float w = 0.f;
            if (d <= rIn - kTaper) w = 1.f;
            else if (d < rIn) w = 0.5f * (1.f + std::cos(3.14159265f * (d - (rIn - kTaper)) / kTaper));
            f.ptr<float>(y)[x] = w * static_cast<float>(gray.ptr<float>(y)[x] - mean);
        }

    const cv::Mat mag = basic::fft2Magnitude(f);
    double total = 0.0, high = 0.0;
    for (int v = 0; v < mag.rows; ++v) {
        const double fy = double(std::min(v, mag.rows - v)) / mag.rows;
        for (int u = 0; u < mag.cols; ++u) {
            if (u == 0 && v == 0) continue;  // DC = mean brightness
            const double fx = double(std::min(u, mag.cols - u)) / mag.cols;
            const double p = double(mag.ptr<float>(v)[u]) * mag.ptr<float>(v)[u];
            total += p;
            if (fx * fx + fy * fy > kHfCutoff * kHfCutoff) high += p;
        }
    }
    return total > 0.0 ? static_cast<float>(high / total) : 0.f;
}

ContrastMeasures measure(const NormalizedCup& cup, const PatternMask& pm) {
    ContrastMeasures m;
    if (pm.area <= 0.f || pm.mask.empty()) return m;

    const cv::Mat inner = basic::erode(cup.cupMask, kRimMargin);
    const cv::Mat foamCore = basic::erode(pm.mask, kCoreMargin);
    const cv::Mat grown = basic::dilate(pm.mask, kCoreMargin);
    cv::Mat cremaCore(inner.rows, inner.cols, CV_8UC1);
    for (int y = 0; y < inner.rows; ++y)
        for (int x = 0; x < inner.cols; ++x)
            cremaCore.ptr<uchar>(y)[x] = (inner.ptr<uchar>(y)[x] && !grown.ptr<uchar>(y)[x]) ? 255 : 0;

    const double foam = meanWhere(cup.gray, foamCore);
    const double crema = meanWhere(cup.gray, cremaCore);
    if (foam < 0.0 || crema < 0.0) return m;  // pattern too thin or covers the whole cup
    if (foam <= crema) return m;              // "foam" not brighter: segmentation went wrong

    m.valid = true;
    m.foam = float(foam);
    m.crema = float(crema);
    m.michelson = float((foam - crema) / std::max(foam + crema, 1e-6));

    const float slope = boundarySlope(cup.gray, pm.mask, inner);
    m.edgeWidth = slope > 1e-6f ? std::min((m.foam - m.crema) / slope, kMaxEdgeWidth) : kMaxEdgeWidth;
    m.hfRatio = highFrequencyRatio(cup.gray, inner, cup);
    return m;
}

std::string fmt(const char* f, double a, double b) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), f, a, b);
    return buf;
}

}  // namespace

ItemScore evalContrast(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask) {
    ItemScore it;
    const ContrastMeasures u = measure(user, userMask);
    const ContrastMeasures r = measure(ref, refMask);
    it.raw = u.michelson;
    it.ref = r.michelson;

    if (!u.valid) {
        it.feedback = userMask.area <= 0.f ? "No pattern detected."
                                           : "Cannot separate foam and crema to measure contrast.";
        return it;
    }
    if (!r.valid) {
        it.feedback = "Reference pattern not usable for contrast.";
        return it;
    }

    const float sMichelson = notWorse(u.michelson, r.michelson, kMichelsonSigma, true);
    const float sEdge = notWorse(u.edgeWidth / std::max(r.edgeWidth, 0.5f), 1.f, kRatioSigma, false);
    const float sHf = r.hfRatio > 0.f ? notWorse(u.hfRatio / r.hfRatio, 1.f, kRatioSigma, true) : 1.f;
    it.score = 0.5f * sMichelson + 0.3f * sEdge + 0.2f * sHf;

    if (sMichelson >= kOk && sEdge >= kOk && sHf >= kOk)
        it.feedback = "Contrast and line definition match the reference.";
    else if (sMichelson <= sEdge && sMichelson <= sHf)
        it.feedback = fmt("Foam and crema contrast is lower than the reference (%.2f vs %.2f).",
                          u.michelson, r.michelson);
    else if (sEdge <= sHf)
        it.feedback = fmt("Boundaries are blurrier than the reference (edge width %.1f px vs %.1f px).",
                          u.edgeWidth, r.edgeWidth);
    else
        it.feedback = fmt("Less fine detail than the reference (high-frequency share %.3f vs %.3f).",
                          u.hfRatio, r.hfRatio);
    return it;
}

}  // namespace la::quality
