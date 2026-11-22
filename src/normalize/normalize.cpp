// Owner: Jaeku Kang
#include "normalize/normalize.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "basic/color.h"
#include "basic/edge.h"
#include "basic/geometry.h"
#include "normalize/manual.h"

namespace la::normalize {
namespace {
constexpr double pi = 3.14159265358979323846;
bool solve5(double a[5][6], double x[5]) {
    for (int c = 0; c < 5; ++c) {
        int best = c;
        for (int r = c + 1; r < 5; ++r)
            if (std::abs(a[r][c]) > std::abs(a[best][c]))
                best = r;
        if (std::abs(a[best][c]) < 1e-10)
            return false;
        for (int j = c; j < 6; ++j)
            std::swap(a[c][j], a[best][j]);
        double d = a[c][c];
        for (int j = c; j < 6; ++j)
            a[c][j] /= d;
        for (int r = 0; r < 5; ++r)
            if (r != c) {
                double f = a[r][c];
                for (int j = c; j < 6; ++j)
                    a[r][j] -= f * a[c][j];
            }
    }
    for (int r = 0; r < 5; ++r)
        x[r] = a[r][5];
    return true;
}
float sample(const cv::Mat &g, float x, float y) {
    return g.ptr<float>(std::clamp(int(std::lround(y)), 0,
                                   g.rows - 1))[std::clamp(int(std::lround(x)), 0, g.cols - 1)];
}
cv::Point2f point(const Ellipse &e, double t, double s = 1) {
    double a = e.angleDeg * pi / 180, u = e.a * std::cos(t) * s, v = e.b * std::sin(t) * s;
    return {float(e.center.x + std::cos(a) * u - std::sin(a) * v),
            float(e.center.y + std::sin(a) * u + std::cos(a) * v)};
}
struct Texture {
    double mean = 0, contrast = 0, homogeneity = 0;
};
// Symmetric horizontal/vertical GLCM; identical fixed quantization in both patches.
Texture texture(const cv::Mat &g, cv::Point2f p, const NormalizeParams &cfg) {
    int n = cfg.glcmLevels, r = cfg.glcmPatchRadius, pixels = 0, pairs = 0;
    std::vector<double> hist(size_t(n) * n, 0);
    Texture t;
    auto level = [n](float v) { return std::clamp(int(v * n), 0, n - 1); };
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x) {
            float v = sample(g, p.x + x, p.y + y);
            t.mean += v;
            ++pixels;
            int a = level(v);
            for (int axis = 0; axis < 2; ++axis) {
                if ((axis == 0 && x == r) || (axis == 1 && y == r))
                    continue;
                int b = level(sample(g, p.x + x + (axis == 0), p.y + y + (axis == 1)));
                hist[a * n + b]++;
                hist[b * n + a]++;
                pairs += 2;
            }
        }
    t.mean /= pixels;
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b) {
            double prob = hist[a * n + b] / pairs, d = double(a - b) / (n - 1);
            t.contrast += prob * d * d;
            t.homogeneity += prob / (1.0 + (a - b) * (a - b));
        }
    return t;
}
struct Candidate {
    Ellipse e;
    std::vector<cv::Point2f> pts;
    float coverage = 0, error = 0, score = 0;
};
std::vector<Candidate> candidates(const cv::Mat &gray, const NormalizeParams &cfg) {
    auto edges = basic::canny(gray, cfg.cannyLow, cfg.cannyHigh);
    int w = gray.cols, h = gray.rows;
    std::vector<unsigned char> seen(size_t(w) * h, 0);
    std::vector<int> queue;
    std::vector<Candidate> out;
    for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) {
            int start = y * w + x;
            if (seen[start] || !edges.ptr<uchar>(y)[x])
                continue;
            queue.clear();
            queue.push_back(start);
            seen[start] = 1;
            for (size_t k = 0; k < queue.size(); ++k) {
                int px = queue[k] % w, py = queue[k] / w;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int xx = px + dx, yy = py + dy;
                        if (xx < 0 || xx >= w || yy < 0 || yy >= h)
                            continue;
                        int q = yy * w + xx;
                        if (!seen[q] && edges.ptr<uchar>(yy)[xx]) {
                            seen[q] = 1;
                            queue.push_back(q);
                        }
                    }
            }
            if (queue.size() < 60)
                continue;
            Candidate c;
            size_t stride = std::max(size_t(1), queue.size() / 2500);
            for (size_t k = 0; k < queue.size(); k += stride)
                c.pts.emplace_back(float(queue[k] % w), float(queue[k] / w));
            try {
                c.e = fitEllipse(c.pts);
            } catch (const std::invalid_argument &) {
                continue;
            }
            auto &e = c.e;
            if (e.b < cfg.minRadiusFraction * std::min(w, h) || e.b / e.a < cfg.minAxisRatio)
                continue;
            double a = e.angleDeg * pi / 180, cs = std::cos(a), sn = std::sin(a);
            double rx = std::hypot(e.a * cs, e.b * sn), ry = std::hypot(e.a * sn, e.b * cs);
            if (e.center.x - rx < 1 || e.center.y - ry < 1 || e.center.x + rx > w - 2 ||
                e.center.y + ry > h - 2)
                continue;
            std::array<bool, 36> sectors{};
            double err = 0;
            for (auto p : c.pts) {
                double dx = p.x - e.center.x, dy = p.y - e.center.y;
                double u = (cs * dx + sn * dy) / e.a, v = (-sn * dx + cs * dy) / e.b;
                double d = std::abs(std::hypot(u, v) - 1);
                err += d;
                if (d < cfg.maxRelativeFitError * 2) {
                    int bin = int((std::atan2(v, u) + pi) * 36 / (2 * pi));
                    sectors[std::clamp(bin, 0, 35)] = true;
                }
            }
            c.error = float(err / c.pts.size());
            c.coverage = float(std::count(sectors.begin(), sectors.end(), true)) / 36;
            if (c.coverage < cfg.minAngularCoverage || c.error > cfg.maxRelativeFitError)
                continue;
            c.score = c.coverage - 4 * c.error + 0.1f * e.b / std::min(w, h);
            out.push_back(std::move(c));
        }
    std::sort(out.begin(), out.end(),
              [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
    if (out.size() > 8)
        out.resize(8);
    return out;
}
std::pair<Ellipse, float> liquidBoundary(const cv::Mat &gray, const Ellipse &rim,
                                         const NormalizeParams &cfg) {
    float best = -std::numeric_limits<float>::infinity(), bestScale = 1;
    std::vector<std::pair<float, float>> scales;
    double margin = (cfg.glcmPatchRadius + 2.0) / rim.b;
    for (double s = cfg.liquidSearchMin; s <= cfg.liquidSearchMax + 1e-6; s += 0.01) {
        std::vector<float> evidence;
        for (int k = 0; k < 48; ++k) {
            double t = 2 * pi * k / 48;
            auto in = texture(gray, point(rim, t, s - margin), cfg),
                 out = texture(gray, point(rim, t, s + margin), cfg);
            double intensity =
                std::abs(out.mean - in.mean) + 0.25 * std::max(0.0, out.mean - in.mean);
            double change = std::abs(in.contrast - out.contrast) +
                            0.25 * std::abs(in.homogeneity - out.homogeneity);
            evidence.push_back(float(intensity + cfg.textureWeight * change));
        }
        std::sort(evidence.begin(), evidence.end());
        float score = 0;
        for (int k = 8; k < 40; ++k)
            score += evidence[k] / 32;
        scales.push_back({float(s), score});
        if (score > best) {
            best = score;
            bestScale = float(s);
        }
    }
    // Ceramic/background can have a stronger edge than coffee/ceramic. Select the
    // innermost *local peak* with comparable circumferential evidence, not the
    // innermost edge pixel or the strongest edge alone.
    for (size_t i = 1; i + 1 < scales.size(); ++i) {
        if (scales[i].second >= best * cfg.innerBoundarySupport &&
            scales[i].second >= scales[i - 1].second && scales[i].second >= scales[i + 1].second) {
            bestScale = scales[i].first;
            best = scales[i].second;
            break;
        }
    }
    Ellipse e = rim;
    e.a *= bestScale;
    e.b *= bestScale;
    return {e, best};
}
void validate(const NormalizeParams &p) {
    if (p.detectionSize < 64 || p.glcmLevels < 2 || p.glcmLevels > 64 || p.glcmPatchRadius < 1 ||
        p.glcmPatchRadius > 16 || !(p.minAxisRatio > 0 && p.minAxisRatio <= 1) ||
        !(p.minRadiusFraction > 0 && p.minRadiusFraction < 0.5f) ||
        !(p.minAngularCoverage > 0 && p.minAngularCoverage <= 1) || !(p.maxRelativeFitError > 0) ||
        !(p.liquidSearchMin > 0 && p.liquidSearchMax >= p.liquidSearchMin &&
          p.liquidSearchMax <= 1.2f) ||
        !(p.innerBoundarySupport > 0 && p.innerBoundarySupport <= 1) ||
        !std::isfinite(p.textureWeight) || p.textureWeight < 0)
        throw std::invalid_argument("invalid NormalizeParams");
}
} // namespace
Ellipse fitEllipse(const std::vector<cv::Point2f> &pts) {
    if (pts.size() < 6)
        throw std::invalid_argument("fitEllipse: insufficient points");
    double mx = 0, my = 0, scale = 0;
    for (auto p : pts) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            throw std::invalid_argument("fitEllipse: nonfinite point");
        mx += p.x;
        my += p.y;
    }
    mx /= pts.size();
    my /= pts.size();
    for (auto p : pts)
        scale += (p.x - mx) * (p.x - mx) + (p.y - my) * (p.y - my);
    scale = std::sqrt(scale / pts.size());
    if (scale < 1e-6)
        throw std::invalid_argument("fitEllipse: degenerate points");
    double normal[5][6]{};
    for (auto p : pts) {
        double x = (p.x - mx) / scale, y = (p.y - my) / scale, v[5] = {x * x, x * y, y * y, x, y};
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j)
                normal[i][j] += v[i] * v[j];
            normal[i][5] += v[i];
        }
    }
    double v[5];
    if (!solve5(normal, v))
        throw std::invalid_argument("fitEllipse: singular fit");
    double A = v[0], B = v[1], C = v[2], D = v[3], E = v[4], det = 4 * A * C - B * B;
    if (det <= 1e-10)
        throw std::invalid_argument("fitEllipse: not an ellipse");
    double cx = (B * E - 2 * C * D) / det, cy = (B * D - 2 * A * E) / det;
    double k = 1 - (A * cx * cx + B * cx * cy + C * cy * cy + D * cx + E * cy);
    double disc = std::hypot(A - C, B), l1 = (A + C - disc) / 2, l2 = (A + C + disc) / 2;
    if (l1 <= 0 || l2 <= 0 || k <= 0)
        throw std::invalid_argument("fitEllipse: invalid axes");
    Ellipse e;
    e.center = {float(mx + cx * scale), float(my + cy * scale)};
    e.a = float(scale * std::sqrt(k / l1));
    e.b = float(scale * std::sqrt(k / l2));
    e.angleDeg = float(0.5 * std::atan2(B, A - C) * 180 / pi + 90);
    if (!std::isfinite(e.a) || !std::isfinite(e.b))
        throw std::invalid_argument("fitEllipse: invalid result");
    return e;
}
std::vector<cv::Point2f> findRimPoints(const cv::Mat &gray) {
    auto c = candidates(gray, NormalizeParams());
    if (c.empty())
        throw std::runtime_error("컵 후보를 찾지 못했습니다.");
    return c.front().pts;
}
CupDetection detectCup(const cv::Mat &bgr, const NormalizeParams &cfg) {
    validate(cfg);
    if (bgr.empty() || bgr.type() != CV_8UC3)
        throw std::invalid_argument("detectCup: expected non-empty BGR image");
    double s = std::min(1.0, double(cfg.detectionSize) / std::max(bgr.cols, bgr.rows));
    auto gray = basic::bgrToGray(bgr);
    if (s < 1)
        gray = basic::warpPerspective(
            gray, {1 / s, 0, 0, 0, 1 / s, 0, 0, 0, 1},
            {std::max(2, int(bgr.cols * s)), std::max(2, int(bgr.rows * s))});
    auto list = candidates(gray, cfg);
    if (list.empty())
        throw std::runtime_error("컵 후보를 찾지 못했습니다.");
    CupDetection result;
    float best = -1e9f;
    for (const auto &c : list) {
        auto liquid = liquidBoundary(gray, c.e, cfg);
        float score = c.score + liquid.second;
        if (score > best) {
            best = score;
            result.rim = c.e;
            result.liquid = liquid.first;
            result.coverage = c.coverage;
            result.fitError = c.error;
        }
    }
    for (Ellipse *e : {&result.rim, &result.liquid}) {
        e->center.x /= float(s);
        e->center.y /= float(s);
        e->a /= float(s);
        e->b /= float(s);
    }
    return result;
}
float patternAxisAngleDeg(const cv::Mat &mask) {
    if (mask.empty() || mask.type() != CV_8UC1)
        throw std::invalid_argument("patternAxisAngleDeg: expected mask");
    double n = 0, mx = 0, my = 0;
    for (int y = 0; y < mask.rows; ++y)
        for (int x = 0; x < mask.cols; ++x)
            if (mask.ptr<uchar>(y)[x]) {
                ++n;
                mx += x;
                my += y;
            }
    if (n < 2)
        return 0;
    mx /= n;
    my /= n;
    double xx = 0, xy = 0, yy = 0;
    for (int y = 0; y < mask.rows; ++y)
        for (int x = 0; x < mask.cols; ++x)
            if (mask.ptr<uchar>(y)[x]) {
                xx += (x - mx) * (x - mx);
                xy += (x - mx) * (y - my);
                yy += (y - my) * (y - my);
            }
    return float(-90 - 0.5 * std::atan2(2 * xy, xx - yy) * 180 / pi);
}
NormalizedCup normalizeCup(const cv::Mat &bgr) { return normalizeCup(bgr, NormalizeParams()); }
NormalizedCup normalizeCup(const cv::Mat &bgr, const NormalizeParams &cfg) {
    const auto e = detectCup(bgr, cfg).liquid;
    // Symmetric affine correction keeps in-plane camera orientation until reference comparison.
    double a = e.angleDeg * pi / 180, cs = std::cos(a), sn = std::sin(a),
           sx = e.a / kCanonicalRadius, sy = e.b / kCanonicalRadius;
    double m00 = sx * cs * cs + sy * sn * sn, m01 = (sx - sy) * cs * sn,
           m11 = sx * sn * sn + sy * cs * cs, c = kCanonicalSize / 2.0;
    basic::Mat3 map{
        m00, m01, e.center.x - c * (m00 + m01), m01, m11, e.center.y - c * (m01 + m11), 0, 0, 1};
    auto cup =
        fromCanonicalImage(basic::warpPerspective(bgr, map, {kCanonicalSize, kCanonicalSize}));
    cup.tiltRatio = e.b / e.a;
    return cup;
}
NormalizedCup rotateCup(const NormalizedCup &cup, float angleDeg) {
    if (!std::isfinite(angleDeg))
        throw std::invalid_argument("rotateCup: invalid angle");
    auto out = fromCanonicalImage(basic::warpPerspective(
        cup.bgr, basic::rotationAbout(cup.center, -angleDeg), {kCanonicalSize, kCanonicalSize}));
    out.tiltRatio = cup.tiltRatio;
    out.axisAngleDeg = cup.axisAngleDeg + angleDeg;
    return out;
}
} // namespace la::normalize
