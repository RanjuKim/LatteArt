// Owner: Ranju Kim
// Foam texture (weight 20%): is the milk foam silky, without bubbles, glossy?
// Measured only on the inside of the foam (boundary excluded, that is contrast):
//   1. GLCM homogeneity (Haralick et al. 1973): neighbouring pixels with similar
//      brightness -> smooth surface. Higher is better.
//   2. bubbles: dark spots on the bright foam found as local maxima of a
//      scale-normalized Laplacian-of-Gaussian (Lindeberg 1998), counted per
//      10,000 foam pixels. Lower is better.
//   3. highlight ratio: share of foam pixels clearly brighter than the foam
//      median (specular shine), using median/MAD so the shine itself does not
//      raise the threshold. Higher is better.
// Known limit: image noise and focus change all three (a noisy photo looks
// rougher, an upscaled low-resolution photo looks smoother).
// Only being worse than the reference is penalized.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "basic/filter.h"
#include "basic/morphology.h"
#include "quality/quality.h"

namespace la::quality {

namespace {

// Draft constants, re-check with real photos.
constexpr int kFoamMargin = 3;          // px removed from the foam edge (GLCM, highlights)
constexpr float kMinFoamPixels = 2000.f; // below this the texture is not judged: with
                                         // fewer pixels one noise spot = many bubbles/10k
constexpr int kGlcmLevels = 16;
constexpr float kGlcmStep = 0.03f;      // gray width of one GLCM level, centered on the
                                        // foam median (so exposure does not shift levels)
constexpr float kLogSigma = 2.f;        // bubbles ~3-6 px across in the 512 frame
constexpr int kBubbleMargin = 6;        // = ceil(3 sigma): the LoG must not reach the edge
constexpr float kBubbleResponse = 0.03f;  // min LoG response (gray units) to count a bubble
constexpr float kHighlightDelta = 0.05f;  // shine: brighter than median + max(3 robust sd, this)
constexpr float kHomogeneitySigma = 0.05f;
constexpr float kBubbleSigma = 3.f;     // bubbles per 10k px
constexpr float kHighlightSigma = 0.02f;
constexpr float kOk = 0.85f;

struct TextureMeasures {
    bool valid = false;
    float homogeneity = 0.f;
    float bubbles = 0.f;    // per 10,000 foam pixels
    int bubbleCount = 0;
    float highlight = 0.f;  // share of foam pixels
};

// Scale-normalized LoG kernel sigma^2 * laplacian(G), shifted to zero sum so
// flat areas give exactly 0. A dark spot on bright foam gives a positive peak.
cv::Mat logKernel(float sigma) {
    const int r = static_cast<int>(std::ceil(3.f * sigma));
    const int n = 2 * r + 1;
    cv::Mat k(n, n, CV_32FC1);
    double sum = 0.0;
    const double s2 = double(sigma) * sigma;
    for (int y = -r; y <= r; ++y)
        for (int x = -r; x <= r; ++x) {
            const double q = (x * x + y * y) / (2.0 * s2);
            const double v = (q - 1.0) / (3.14159265358979 * s2) * std::exp(-q);  // sigma^2 * LoG
            k.ptr<float>(y + r)[x + r] = static_cast<float>(v);
            sum += v;
        }
    const float mean = static_cast<float>(sum / (n * n));
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) k.ptr<float>(y)[x] -= mean;
    return k;
}

float glcmHomogeneity(const cv::Mat& gray, const cv::Mat& foam, float median) {
    // Co-occurrence of (pixel, right neighbour) and (pixel, lower neighbour),
    // both inside the foam, symmetric. Levels are kGlcmStep wide and centered
    // on the foam median, so a brighter or darker photo gives the same levels.
    double P[kGlcmLevels][kGlcmLevels] = {};
    double total = 0.0;
    auto level = [median](float v) {
        const int l = static_cast<int>(std::floor((v - median) / kGlcmStep)) + kGlcmLevels / 2;
        return std::clamp(l, 0, kGlcmLevels - 1);
    };
    for (int y = 0; y < gray.rows; ++y)
        for (int x = 0; x < gray.cols; ++x) {
            if (!foam.ptr<uchar>(y)[x]) continue;
            const int a = level(gray.ptr<float>(y)[x]);
            if (x + 1 < gray.cols && foam.ptr<uchar>(y)[x + 1]) {
                const int b = level(gray.ptr<float>(y)[x + 1]);
                P[a][b] += 1; P[b][a] += 1; total += 2;
            }
            if (y + 1 < gray.rows && foam.ptr<uchar>(y + 1)[x]) {
                const int b = level(gray.ptr<float>(y + 1)[x]);
                P[a][b] += 1; P[b][a] += 1; total += 2;
            }
        }
    if (total == 0.0) return 0.f;
    double h = 0.0;
    for (int i = 0; i < kGlcmLevels; ++i)
        for (int j = 0; j < kGlcmLevels; ++j) h += P[i][j] / total / (1.0 + std::abs(i - j));
    return static_cast<float>(h);
}

int countBubbles(const cv::Mat& gray, const cv::Mat& foam) {
    const cv::Mat resp = basic::convolve2D(gray, logKernel(kLogSigma));
    int count = 0;
    for (int y = 1; y < gray.rows - 1; ++y)
        for (int x = 1; x < gray.cols - 1; ++x) {
            if (!foam.ptr<uchar>(y)[x]) continue;
            const float v = resp.ptr<float>(y)[x];
            if (v < kBubbleResponse) continue;
            bool isMax = true;  // strict 3x3 local maximum (ties broken by position)
            for (int dy = -1; dy <= 1 && isMax; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy) continue;
                    const float w = resp.ptr<float>(y + dy)[x + dx];
                    if (w > v || (w == v && (dy < 0 || (dy == 0 && dx < 0)))) { isMax = false; break; }
                }
            count += isMax;
        }
    return count;
}

float median(std::vector<float> v) {
    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    return v[mid];
}

TextureMeasures measure(const NormalizedCup& cup, const PatternMask& pm) {
    TextureMeasures m;
    if (pm.area <= 0.f || pm.mask.empty()) return m;
    const cv::Mat foam = basic::erode(pm.mask, kFoamMargin);

    std::vector<float> values;
    for (int y = 0; y < foam.rows; ++y)
        for (int x = 0; x < foam.cols; ++x)
            if (foam.ptr<uchar>(y)[x]) values.push_back(cup.gray.ptr<float>(y)[x]);
    if (values.size() < kMinFoamPixels) return m;
    m.valid = true;
    const double n = static_cast<double>(values.size());

    // Robust brightness statistics: highlights are outliers, so they must not
    // move the baseline (with mean + 2 sd, more shine raised the threshold and
    // a glossier foam could count as LESS glossy).
    const float med = median(values);
    std::vector<float> dev(values.size());
    for (size_t i = 0; i < values.size(); ++i) dev[i] = std::abs(values[i] - med);
    const float robustSd = 1.4826f * median(dev);  // MAD scaled to a Gaussian sd
    const float shine = med + std::max(3.f * robustSd, kHighlightDelta);
    double bright = 0.0;
    for (float v : values) bright += v > shine;

    // Bubbles only where the whole LoG kernel stays inside the foam.
    const cv::Mat bubbleArea = basic::erode(pm.mask, kBubbleMargin);
    double bubblePixels = 0.0;
    for (int y = 0; y < bubbleArea.rows; ++y)
        for (int x = 0; x < bubbleArea.cols; ++x) bubblePixels += bubbleArea.ptr<uchar>(y)[x] != 0;

    m.homogeneity = glcmHomogeneity(cup.gray, foam, med);
    m.bubbleCount = bubblePixels > 0 ? countBubbles(cup.gray, bubbleArea) : 0;
    m.bubbles = bubblePixels > 0 ? static_cast<float>(m.bubbleCount * 10000.0 / bubblePixels) : 0.f;
    m.highlight = static_cast<float>(bright / n);
    return m;
}

std::string fmt(const char* f, double a, double b) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), f, a, b);
    return buf;
}

}  // namespace

ItemScore evalTexture(const NormalizedCup& user, const PatternMask& userMask,
                      const NormalizedCup& ref, const PatternMask& refMask) {
    ItemScore it;
    const TextureMeasures u = measure(user, userMask);
    const TextureMeasures r = measure(ref, refMask);
    it.raw = u.homogeneity;
    it.ref = r.homogeneity;

    if (!u.valid) {
        it.feedback = userMask.area <= 0.f ? "No pattern detected."
                                           : "Foam area too small to judge the texture.";
        return it;
    }
    if (!r.valid) {
        it.feedback = "Reference foam area too small to judge the texture.";
        return it;
    }

    const float sSmooth = notWorse(u.homogeneity, r.homogeneity, kHomogeneitySigma, true);
    const float sBubble = notWorse(u.bubbles, r.bubbles, kBubbleSigma, false);
    const float sGloss = notWorse(u.highlight, r.highlight, kHighlightSigma, true);
    it.score = 0.4f * sSmooth + 0.4f * sBubble + 0.2f * sGloss;

    if (sSmooth >= kOk && sBubble >= kOk && sGloss >= kOk)
        it.feedback = "Foam texture matches the reference.";
    else if (sBubble <= sSmooth && sBubble <= sGloss)
        it.feedback = fmt("Visible bubbles in the foam (%.0f found, %.1f per 10k px", u.bubbleCount, u.bubbles) +
                      fmt(" vs %.1f in the reference).", r.bubbles, 0.0);
    else if (sSmooth <= sGloss)
        it.feedback = fmt("Foam surface is rougher than the reference (homogeneity %.3f vs %.3f).",
                          u.homogeneity, r.homogeneity);
    else
        it.feedback = fmt("Foam looks less glossy than the reference (highlights %.1f%% vs %.1f%%).",
                          100.0 * u.highlight, 100.0 * r.highlight);
    return it;
}

}  // namespace la::quality
