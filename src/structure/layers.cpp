// Owner: Jaeku Kang
#include "structure/layers.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include "basic/fft.h"
#include "scoring/scoring.h"

namespace la::structure {
namespace {
constexpr float pi = 3.14159265358979323846f;
constexpr int frame = 256, margin = 16, extent = frame - 2 * margin;
struct Bounds {
    float left = 1e9f, top = 1e9f, right = -1e9f, bottom = -1e9f;
    float width() const { return right - left; }
    float height() const { return bottom - top; }
};
void validate(const LayerParams &p) {
    if (p.columnHalfWidth < 0 || p.columnHalfWidth > 1024 || p.scanStep < 1 ||
        p.scanStep > extent || p.stripHalfWidth < 0 || p.stripHalfWidth > 10 ||
        p.smoothRadius < 0 || p.smoothRadius > 20 || p.minPeakDistance < 1 ||
        !(p.positionSigma > 0) || !std::isfinite(p.positionSigma) || !(p.spacingSigma > 0) ||
        !std::isfinite(p.spacingSigma) || !(p.thicknessSigma > 0) ||
        !std::isfinite(p.thicknessSigma) || !(p.gapCost > 0) || !std::isfinite(p.gapCost) ||
        !(p.minProminence > 0 && p.minProminence <= 1) || !(p.sizeLimit > 0 && p.sizeLimit < 1) ||
        p.coarseAngleStep < 1 || p.coarseAngleStep > 90 || p.refineAngleStep < 1 ||
        p.refineAngleStep > p.coarseAngleStep || p.fineAngleStep < 1 ||
        p.fineAngleStep > p.refineAngleStep || p.rotationCandidates < 1 || p.rotationCandidates > 8)
        throw std::invalid_argument("invalid LayerParams");
}
void validateCup(const NormalizedCup &cup, const PatternMask &pm) {
    if (cup.gray.empty() || cup.gray.type() != CV_32FC1 || pm.mask.empty() ||
        pm.mask.type() != CV_8UC1 || cup.gray.rows != pm.mask.rows || cup.gray.cols != pm.mask.cols)
        throw std::invalid_argument(
            "structure: grayscale image and same-size binary mask required");
}
Bounds bounds(const cv::Mat &mask, float angle) {
    Bounds b;
    float a = angle * pi / 180, cs = std::cos(a), sn = std::sin(a);
    for (int y = 0; y < mask.rows; ++y)
        for (int x = 0; x < mask.cols; ++x)
            if (mask.ptr<uchar>(y)[x]) {
                // Rotate all four pixel corners so width/height are exact at multiples of 90
                // degrees.
                for (int dy = 0; dy <= 1; ++dy)
                    for (int dx = 0; dx <= 1; ++dx) {
                        float u = cs * (x + dx) - sn * (y + dy), v = sn * (x + dx) + cs * (y + dy);
                        b.left = std::min(b.left, u);
                        b.right = std::max(b.right, u);
                        b.top = std::min(b.top, v);
                        b.bottom = std::max(b.bottom, v);
                    }
            }
    if (b.width() <= 0 || b.height() <= 0)
        throw std::runtime_error("패턴을 찾지 못했습니다.");
    return b;
}
float pixel(const cv::Mat &g, float x, float y) {
    if (x < 0 || y < 0 || x > g.cols - 1 || y > g.rows - 1)
        return 0;
    int x0 = int(x), y0 = int(y), x1 = std::min(x0 + 1, g.cols - 1),
        y1 = std::min(y0 + 1, g.rows - 1);
    float fx = x - x0, fy = y - y0;
    return (1 - fy) * ((1 - fx) * g.ptr<float>(y0)[x0] + fx * g.ptr<float>(y0)[x1]) +
           fy * ((1 - fx) * g.ptr<float>(y1)[x0] + fx * g.ptr<float>(y1)[x1]);
}
struct Frame {
    cv::Mat gray, mask;
    Bounds box;
};
Frame makeFrame(const NormalizedCup &cup, const PatternMask &pm, float angle) {
    Frame f;
    f.box = bounds(pm.mask, angle);
    f.gray = cv::Mat(frame, frame, CV_32FC1);
    f.mask = cv::Mat(frame, frame, CV_8UC1);
    float a = angle * pi / 180, cs = std::cos(a), sn = std::sin(a);
    for (int y = 0; y < frame; ++y)
        for (int x = 0; x < frame; ++x) {
            float u = f.box.left + (x - margin + 0.5f) * f.box.width() / extent;
            float v = f.box.top + (y - margin + 0.5f) * f.box.height() / extent;
            float sx = cs * u + sn * v, sy = -sn * u + cs * v;
            f.gray.ptr<float>(y)[x] = pixel(cup.gray, sx - 0.5f, sy - 0.5f);
            int ix = int(std::floor(sx)), iy = int(std::floor(sy));
            f.mask.ptr<uchar>(y)[x] = (ix >= 0 && iy >= 0 && ix < pm.mask.cols && iy < pm.mask.rows)
                                          ? pm.mask.ptr<uchar>(iy)[ix]
                                          : 0;
        }
    return f;
}
std::vector<float> smooth(const std::vector<float> &v, int radius) {
    std::vector<float> out(v.size());
    for (int i = 0; i < int(v.size()); ++i) {
        double sum = 0;
        int n = 0;
        for (int j = std::max(0, i - radius); j <= std::min(int(v.size()) - 1, i + radius); ++j) {
            sum += v[j];
            ++n;
        }
        out[i] = float(sum / n);
    }
    return out;
}
std::vector<Peak> extract(const std::vector<float> &profile,
                          const std::vector<unsigned char> &support, const LayerParams &p) {
    auto v = smooth(profile, p.smoothRadius);
    auto peaks = findPeaks(v, p.minProminence);
    // Suppress nearby duplicate peaks by strength, not by traversal order.
    std::sort(peaks.begin(), peaks.end(), [&](int a, int b) { return v[a] > v[b]; });
    std::vector<int> kept;
    for (int i : peaks) {
        bool on = false;
        for (int j = std::max(0, i - p.smoothRadius);
             j <= std::min(int(v.size()) - 1, i + p.smoothRadius); ++j)
            on = on || support[j];
        if (!on)
            continue;
        bool close = false;
        for (int j : kept)
            if (std::abs(i - j) < p.minPeakDistance)
                close = true;
        if (!close)
            kept.push_back(i);
    }
    std::sort(kept.begin(), kept.end());
    std::vector<Peak> out;
    for (int i : kept) {
        float lowL = v[i], lowR = v[i];
        for (int j = i - 1; j >= 0; --j) {
            if (v[j] > v[i] + 1e-5f)
                break;
            lowL = std::min(lowL, v[j]);
        }
        for (int j = i + 1; j < int(v.size()); ++j) {
            if (v[j] > v[i] + 1e-5f)
                break;
            lowR = std::min(lowR, v[j]);
        }
        float half = (v[i] + std::max(lowL, lowR)) * 0.5f;
        int l = i, r = i;
        while (l > 0 && v[l - 1] >= half)
            --l;
        while (r + 1 < int(v.size()) && v[r + 1] >= half)
            ++r;
        float left = float(l), right = float(r);
        if (l > 0 && v[l] > v[l - 1])
            left = l - (v[l] - half) / (v[l] - v[l - 1]);
        if (r + 1 < int(v.size()) && v[r] > v[r + 1])
            right = r + (v[r] - half) / (v[r] - v[r + 1]);
        out.push_back({(i - margin + 0.5f) / extent, std::max(1.f, right - left) / extent});
    }
    return out;
}
struct Scan {
    std::vector<Peak> peaks;
    bool hasPattern = false;
};
Scan scan(const Frame &f, int x, const LayerParams &p) {
    std::vector<float> profile(frame);
    std::vector<unsigned char> support(frame, 0);
    Scan out;
    for (int y = 0; y < frame; ++y) {
        int n = 0;
        for (int xx = std::max(0, x - p.stripHalfWidth);
             xx <= std::min(frame - 1, x + p.stripHalfWidth); ++xx) {
            profile[y] += f.gray.ptr<float>(y)[xx];
            support[y] |= f.mask.ptr<uchar>(y)[xx];
            ++n;
        }
        profile[y] /= n;
        out.hasPattern = out.hasPattern || support[y] != 0;
    }
    out.peaks = extract(profile, support, p);
    return out;
}
float relative(float a, float b) { return std::abs(a - b) / std::max(a, 1.f / extent); }
float wrapped(float a) {
    a = std::fmod(a, 360.f);
    return a < 0 ? a + 360 : a;
}
float angleDistance(float a, float b) {
    float d = std::abs(wrapped(a) - wrapped(b));
    return std::min(d, 360 - d);
}
StructureComparison compareFrames(const Frame &ref, const Frame &user, float angle,
                                  const LayerParams &p) {
    StructureComparison out;
    out.angleDeg = wrapped(angle);
    out.referenceWidth = ref.box.width();
    out.referenceHeight = ref.box.height();
    out.userWidth = user.box.width();
    out.userHeight = user.box.height();
    out.widthDifference = std::abs(out.userWidth / out.referenceWidth - 1);
    out.heightDifference = std::abs(out.userHeight / out.referenceHeight - 1);
    bool width = out.widthDifference + 1e-6f >= p.sizeLimit,
         height = out.heightDifference + 1e-6f >= p.sizeLimit;
    out.sizeOk = !width && !height;
    if (width && height)
        out.sizeMessage = "크기가 맞지 않습니다.";
    else if (width)
        out.sizeMessage = "가로폭 크기가 맞지 않습니다.";
    else if (height)
        out.sizeMessage = "세로폭 크기가 맞지 않습니다.";
    for (int x = margin; x < frame - margin; x += p.scanStep) {
        auto r = scan(ref, x, p), u = scan(user, x, p);
        if (!r.hasPattern && !u.hasPattern)
            continue;
        auto line = matchLayers(r.peaks, u.peaks, p);
        line.x = float(x - margin + 0.5f) / extent;
        // No measurable brightness peaks on a selected line is not a perfect match.
        if (r.peaks.empty() && u.peaks.empty()) {
            line.U = 0;
            line.bandLow = 0;
            line.bandHigh = 29;
        }
        out.alignmentScore += line.bandLow + (line.bandHigh - line.bandLow) * line.U;
        out.lines.push_back(std::move(line));
    }
    if (out.lines.empty())
        throw std::runtime_error("비교할 패턴 선이 없습니다.");
    out.alignmentScore /= out.lines.size();
    return out;
}
} // namespace

std::vector<int> findPeaks(const std::vector<float> &v, float prominence) {
    if (!std::isfinite(prominence) || prominence <= 0)
        throw std::invalid_argument("findPeaks: positive prominence required");
    for (float f : v)
        if (!std::isfinite(f))
            throw std::invalid_argument("findPeaks: nonfinite profile");
    std::vector<int> out;
    for (int i = 1; i + 1 < int(v.size()); ++i) {
        if (v[i] <= v[i - 1] + 1e-5f)
            continue;
        int end = i;
        while (end + 1 < int(v.size()) && std::abs(v[end + 1] - v[i]) <= 1e-5f)
            ++end;
        if (end + 1 == int(v.size()) || v[end + 1] >= v[i] - 1e-5f) {
            i = end;
            continue;
        }
        float left = v[i], right = v[i];
        for (int j = i - 1; j >= 0; --j) {
            if (v[j] > v[i] + 1e-5f)
                break;
            left = std::min(left, v[j]);
        }
        for (int j = end + 1; j < int(v.size()); ++j) {
            if (v[j] > v[i] + 1e-5f)
                break;
            right = std::min(right, v[j]);
        }
        if (v[i] - std::max(left, right) >= prominence)
            out.push_back((i + end) / 2);
        i = end;
    }
    return out;
}
float coefficientOfVariation(const std::vector<float> &v) {
    if (v.size() < 2)
        return 0;
    double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    if (mean <= 0)
        return 0;
    double var = 0;
    for (float x : v)
        var += (x - mean) * (x - mean);
    return float(std::sqrt(var / v.size()) / mean);
}
std::vector<float> axisProfile(const NormalizedCup &cup, const PatternMask &pm, int halfWidth) {
    validateCup(cup, pm);
    if (halfWidth < 0)
        throw std::invalid_argument("negative profile half width");
    auto b = bounds(pm.mask, 0);
    int center = int((b.left + b.right) / 2);
    std::vector<float> out(cup.gray.rows);
    for (int y = 0; y < cup.gray.rows; ++y) {
        int n = 0;
        for (int x = std::max(0, center - halfWidth);
             x <= std::min(cup.gray.cols - 1, center + halfWidth); ++x) {
            out[y] += cup.gray.ptr<float>(y)[x];
            ++n;
        }
        out[y] /= n;
    }
    return out;
}
LayerResult analyzeLayers(const NormalizedCup &cup, const PatternMask &pm, const LayerParams &p) {
    validate(p);
    LayerResult r;
    r.profile = axisProfile(cup, pm, p.columnHalfWidth);
    auto box = bounds(pm.mask, 0);
    int center = int((box.left + box.right) / 2);
    std::vector<unsigned char> support(r.profile.size(), 0);
    for (int y = 0; y < pm.mask.rows; ++y)
        for (int x = std::max(0, center - p.columnHalfWidth);
             x <= std::min(pm.mask.cols - 1, center + p.columnHalfWidth); ++x)
            support[y] |= pm.mask.ptr<uchar>(y)[x];
    auto peaks = extract(r.profile, support, p);
    for (auto peak : peaks) {
        r.peakPos.push_back(peak.position * extent + margin - .5f);
        r.fwhm.push_back(peak.width * extent);
    }
    r.count = int(peaks.size());
    for (size_t i = 1; i < r.peakPos.size(); ++i)
        r.spacings.push_back(r.peakPos[i] - r.peakPos[i - 1]);
    r.spacingCV = coefficientOfVariation(r.spacings);
    r.thicknessCV = coefficientOfVariation(r.fwhm);
    // Crop to pattern support: zero padding outside the cup must not become a period.
    int first = std::clamp(int(box.top), 0, int(r.profile.size()) - 1);
    int last = std::clamp(int(std::ceil(box.bottom)), first + 1, int(r.profile.size()));
    auto profile = smooth(std::vector<float>(r.profile.begin() + first, r.profile.begin() + last),
                          p.smoothRadius);
    // Legacy single-axis diagnostic only; never used for the multi-line final score.
    float mean = std::accumulate(profile.begin(), profile.end(), 0.f) / profile.size();
    for (float &v : profile)
        v -= mean;
    size_t padded = 0;
    auto mag = basic::magnitudeSpectrum(profile, &padded);
    if (mag.size() > 1 && *std::max_element(mag.begin() + 1, mag.end()) > 1e-6) {
        auto k = std::max_element(mag.begin() + 1, mag.end()) - mag.begin();
        r.fftCount = int(std::lround(double(k) * profile.size() / padded));
    }
    r.uncertain = r.count != r.fftCount;
    r.U = r.count ? std::clamp(1.f - .5f * (r.spacingCV + r.thicknessCV), 0.f, 1.f) : 0.f;
    return r;
}
LineComparison matchLayers(const std::vector<Peak> &ref, const std::vector<Peak> &user,
                           const LayerParams &p) {
    validate(p);
    auto check = [](const std::vector<Peak> &peaks) {
        float last = -1e9f;
        for (auto v : peaks) {
            if (!std::isfinite(v.position) || !std::isfinite(v.width) || v.width <= 0 ||
                v.position < last)
                throw std::invalid_argument("matchLayers: invalid/unsorted peaks");
            last = v.position;
        }
    };
    check(ref);
    check(user);
    const int n = int(ref.size()), m = int(user.size()), stride = m + 1;
    std::vector<float> dp(size_t(n + 1) * (m + 1));
    std::vector<char> action(dp.size());
    for (int i = 1; i <= n; ++i) {
        dp[i * stride] = i * p.gapCost;
        action[i * stride] = 'D';
    }
    for (int j = 1; j <= m; ++j) {
        dp[j] = j * p.gapCost;
        action[j] = 'I';
    }
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j) {
            float cost = std::abs(ref[i - 1].position - user[j - 1].position) / p.positionSigma +
                         0.35f * relative(ref[i - 1].width, user[j - 1].width) / p.thicknessSigma;
            int q = i * stride + j;
            dp[q] = dp[q - stride - 1] + cost;
            action[q] = 'M';
            if (dp[q - stride] + p.gapCost < dp[q]) {
                dp[q] = dp[q - stride] + p.gapCost;
                action[q] = 'D';
            }
            if (dp[q - 1] + p.gapCost < dp[q]) {
                dp[q] = dp[q - 1] + p.gapCost;
                action[q] = 'I';
            }
        }
    LineComparison out;
    out.reference = ref;
    out.user = user;
    for (int i = n, j = m; i || j;) {
        char a = action[i * stride + j];
        if (a == 'M') {
            out.matches.push_back({i - 1, j - 1});
            --i;
            --j;
        } else if (a == 'D') {
            ++out.missing;
            --i;
        } else {
            ++out.extra;
            --j;
        }
    }
    std::reverse(out.matches.begin(), out.matches.end());
    // Equal counts can still contain a missing layer plus an extra unrelated layer.
    auto band = scoring::bandFor(std::max(out.missing, out.extra));
    out.bandLow = band.low;
    out.bandHigh = band.high;
    if (out.matches.empty()) {
        out.bandLow = 0;
        out.bandHigh = 29;
        return out;
    }
    for (auto a : out.matches)
        out.thicknessScore +=
            std::exp(-relative(ref[a.reference].width, user[a.user].width) / p.thicknessSigma);
    out.thicknessScore /= out.matches.size();
    if (out.matches.size() == 1)
        out.spacingScore = 1; // not measurable: neutral, count errors remain in band
    else {
        for (size_t k = 1; k < out.matches.size(); ++k) {
            auto a = out.matches[k - 1], b = out.matches[k];
            float rd = ref[b.reference].position - ref[a.reference].position,
                  ud = user[b.user].position - user[a.user].position;
            out.spacingScore += std::exp(-relative(rd, ud) / p.spacingSigma);
        }
        out.spacingScore /= out.matches.size() - 1;
    }
    out.U = 0.5f * (out.spacingScore + out.thicknessScore);
    return out;
}
StructureComparison compareAtAngle(const NormalizedCup &ref, const PatternMask &rm,
                                   const NormalizedCup &user, const PatternMask &um, float angle,
                                   const LayerParams &p) {
    validate(p);
    validateCup(ref, rm);
    validateCup(user, um);
    if (!std::isfinite(angle))
        throw std::invalid_argument("invalid rotation angle");
    return compareFrames(makeFrame(ref, rm, 0), makeFrame(user, um, angle), angle, p);
}
StructureComparison compareStructure(const NormalizedCup &ref, const PatternMask &rm,
                                     const NormalizedCup &user, const PatternMask &um,
                                     const LayerParams &p) {
    validate(p);
    validateCup(ref, rm);
    validateCup(user, um);
    auto fixed = makeFrame(ref, rm, 0);
    std::vector<StructureComparison> coarse;
    auto evaluate = [&](float a) { return compareFrames(fixed, makeFrame(user, um, a), a, p); };
    for (int a = 0; a < 360; a += p.coarseAngleStep)
        coarse.push_back(evaluate(float(a)));
    std::stable_sort(coarse.begin(), coarse.end(), [](const auto &a, const auto &b) {
        return a.alignmentScore > b.alignmentScore;
    });
    std::vector<StructureComparison> seeds;
    for (const auto &c : coarse) {
        bool separated = true;
        for (const auto &s : seeds)
            if (angleDistance(c.angleDeg, s.angleDeg) < 30)
                separated = false;
        if (separated)
            seeds.push_back(c);
        if (int(seeds.size()) == p.rotationCandidates)
            break;
    }
    auto best = coarse.front();
    for (auto seed : seeds) {
        float center = seed.angleDeg;
        for (int d = -p.coarseAngleStep; d <= p.coarseAngleStep; d += p.refineAngleStep) {
            auto c = evaluate(center + d);
            if (c.alignmentScore > seed.alignmentScore + 1e-5f)
                seed = std::move(c);
        }
        center = seed.angleDeg;
        for (int d = -p.refineAngleStep; d <= p.refineAngleStep; d += p.fineAngleStep) {
            auto c = evaluate(center + d);
            if (c.alignmentScore > seed.alignmentScore + 1e-5f)
                seed = std::move(c);
        }
        if (seed.alignmentScore > best.alignmentScore + 1e-5f)
            best = std::move(seed);
    }
    return best;
}
void requireMatchingSize(const StructureComparison &result) {
    if (!result.sizeOk)
        throw std::runtime_error(result.sizeMessage.empty() ? "크기가 맞지 않습니다."
                                                            : result.sizeMessage);
}
} // namespace la::structure
