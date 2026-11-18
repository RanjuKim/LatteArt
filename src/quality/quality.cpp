// Owner: Ranju Kim
#include "quality/quality.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace la::quality {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kMinSlices = 10;
constexpr int kMinSlicePixels = 3;  // thinner slices (tips) are too noisy

struct Pt {
    double x, y;  // pixel centers
};

std::vector<Pt> maskPixels(const cv::Mat& mask) {
    std::vector<Pt> pts;
    if (mask.empty()) return pts;
    for (int y = 0; y < mask.rows; ++y) {
        const uchar* p = mask.ptr<uchar>(y);
        for (int x = 0; x < mask.cols; ++x)
            if (p[x]) pts.push_back({x + 0.5, y + 0.5});
    }
    return pts;
}

// Unit vector along the axis (d) and to its right (n), image coordinates.
void axisFrame(float angleDeg, double& dx, double& dy, double& nx, double& ny) {
    const double a = angleDeg * kPi / 180.0;
    dx = std::sin(a);  dy = -std::cos(a);  // 0 deg = up
    nx = std::cos(a);  ny = std::sin(a);   // 0 deg = right
}

// IoU with the mirror image, using the mask pixel list. Reflection is its own
// inverse, so |mirror| = |mask| up to nearest-pixel rounding and
// union = 2 |mask| - intersection.
float mirrorIoUPixels(const cv::Mat& mask, const std::vector<Pt>& pts, const AxisLine& axis) {
    if (pts.empty()) return 0.f;
    double dx, dy, nx, ny;
    axisFrame(axis.angleDeg, dx, dy, nx, ny);
    double inter = 0.0;
    for (const Pt& p : pts) {
        const double rx = p.x - axis.origin.x, ry = p.y - axis.origin.y;
        const double v = rx * nx + ry * ny;           // distance to the right of the axis
        const double mx = p.x - 2.0 * v * nx, my = p.y - 2.0 * v * ny;
        const int ix = static_cast<int>(std::floor(mx)), iy = static_cast<int>(std::floor(my));
        if (ix >= 0 && iy >= 0 && ix < mask.cols && iy < mask.rows && mask.ptr<uchar>(iy)[ix]) inter += 1.0;
    }
    const double uni = 2.0 * pts.size() - inter;
    return static_cast<float>(inter / uni);
}

cv::Point2f centroid(const std::vector<Pt>& pts) {
    double sx = 0, sy = 0;
    for (const Pt& p : pts) { sx += p.x; sy += p.y; }
    return {static_cast<float>(sx / pts.size()), static_cast<float>(sy / pts.size())};
}

}  // namespace

float mirrorIoU(const cv::Mat& mask, const AxisLine& axis) {
    return mirrorIoUPixels(mask, maskPixels(mask), axis);
}

AxisLine findSymmetryAxis(const cv::Mat& mask, float guessDeg, float searchDeg) {
    const std::vector<Pt> pts = maskPixels(mask);
    AxisLine best;
    best.angleDeg = guessDeg;
    if (pts.empty()) return best;
    best.origin = centroid(pts);

    // Coarse 1-degree search, then 0.1-degree refinement around the winner.
    float bestIoU = -1.f;
    auto tryAngle = [&](float deg) {
        const AxisLine cand{best.origin, deg};
        const float v = mirrorIoUPixels(mask, pts, cand);
        if (v > bestIoU) { bestIoU = v; best.angleDeg = deg; }
    };
    const float lo = guessDeg - searchDeg, hi = guessDeg + searchDeg;
    for (float d = lo; d <= hi + 1e-3f; d += 1.f) tryAngle(d);
    // Refinement stays inside the search window.
    const float coarse = best.angleDeg;
    for (float d = std::max(coarse - 1.f, lo); d <= std::min(coarse + 1.f, hi) + 1e-3f; d += 0.1f) tryAngle(d);
    return best;
}

namespace {

// Slice the mask across the axis and fit v = a + b u to the slice middles.
// The middle is taken between the outer edges of each slice, so a crema hole
// or the two lobes of a heart do not move it. For slices with at least
// kRobustSlicePixels pixels the edges are the 5% / 95% quantiles, so a small
// drip stuck to the pattern does not pull the middle sideways.
constexpr int kRobustSlicePixels = 20;

struct MidlineFit {
    bool valid = false;
    int slices = 0;
    double slope = 0.0;  // b
    double rmsPx = 0.0;  // RMS residual of the middles
};

MidlineFit fitMidline(const std::vector<Pt>& pts, const AxisLine& axis) {
    MidlineFit f;
    double dx, dy, nx, ny;
    axisFrame(axis.angleDeg, dx, dy, nx, ny);

    std::map<int, std::vector<double>> slices;  // key: 1-px step along the axis -> v values
    for (const Pt& p : pts) {
        const double rx = p.x - axis.origin.x, ry = p.y - axis.origin.y;
        const double u = rx * dx + ry * dy, v = rx * nx + ry * ny;
        slices[static_cast<int>(std::floor(u))].push_back(v);
    }

    std::vector<double> us, vs;
    for (auto& [k, v] : slices) {
        const int n = static_cast<int>(v.size());
        if (n < kMinSlicePixels) continue;
        double lo, hi;
        if (n >= kRobustSlicePixels) {
            const int iLo = n / 20, iHi = n - 1 - n / 20;
            std::nth_element(v.begin(), v.begin() + iLo, v.end());
            lo = v[iLo];
            std::nth_element(v.begin(), v.begin() + iHi, v.end());
            hi = v[iHi];
        } else {
            lo = *std::min_element(v.begin(), v.end());
            hi = *std::max_element(v.begin(), v.end());
        }
        us.push_back(k + 0.5);
        vs.push_back(0.5 * (lo + hi));
    }
    f.slices = static_cast<int>(us.size());
    if (f.slices < kMinSlices) return f;

    double su = 0, sv = 0, suu = 0, suv = 0;
    const double n = static_cast<double>(us.size());
    for (size_t i = 0; i < us.size(); ++i) {
        su += us[i]; sv += vs[i]; suu += us[i] * us[i]; suv += us[i] * vs[i];
    }
    const double den = n * suu - su * su;
    if (den <= 0.0) return f;
    f.slope = (n * suv - su * sv) / den;
    const double a = (sv - f.slope * su) / n;
    double rss = 0.0;
    for (size_t i = 0; i < us.size(); ++i) {
        const double e = vs[i] - (a + f.slope * us[i]);
        rss += e * e;
    }
    f.rmsPx = std::sqrt(rss / n);
    f.valid = true;
    return f;
}

}  // namespace

// The slice middles lie on the symmetry axis only when the slices are cut
// perpendicular to it; slicing a tilted WIDE pattern (heart, ellipse) along
// another direction makes the middles follow a different line, which looked
// like a lean the wrong way plus a bend. So pass the pattern's own axis
// (findSymmetryAxis); then tiltDeg is only the small remaining correction.
AxisShape measureAxis(const cv::Mat& mask, const AxisLine& axis) {
    AxisShape out;
    const std::vector<Pt> pts = maskPixels(mask);
    if (pts.empty()) return out;

    const MidlineFit fit = fitMidline(pts, axis);
    out.slices = fit.slices;
    if (!fit.valid) return out;
    out.valid = true;
    out.tiltDeg = static_cast<float>(std::atan(fit.slope) * 180.0 / kPi);
    out.bendPx = static_cast<float>(fit.rmsPx);
    return out;
}

float closeness(float user, float ref, float sigma) {
    if (sigma <= 0.f) return user == ref ? 1.f : 0.f;
    return std::exp(-std::abs(user - ref) / sigma);
}

float notWorse(float user, float ref, float sigma, bool higherIsBetter) {
    const bool atLeastAsGood = higherIsBetter ? user >= ref : user <= ref;
    return atLeastAsGood ? 1.f : closeness(user, ref, sigma);
}

QualityResult evaluateQuality(const NormalizedCup& user, const PatternMask& userMask,
                              const NormalizedCup& ref, const PatternMask& refMask,
                              const Weights& w) {
    QualityResult q;
    q.contrast = evalContrast(user, userMask, ref, refMask);
    q.shape = evalShape(userMask, refMask);
    q.texture = evalTexture(user, userMask, ref, refMask);
    q.position = evalPosition(user, userMask, ref, refMask);
    q.size = evalSize(user, userMask, ref, refMask);
    q.Q = w.contrast * q.contrast.score + w.shape * q.shape.score +
          w.texture * q.texture.score + w.position * q.position.score + w.size * q.size.score;
    return q;
}

}  // namespace la::quality
