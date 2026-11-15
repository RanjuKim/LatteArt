// Owner: Ranju Kim
#include "basic/threshold.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace la::basic {

namespace {

constexpr int kBins = 256;

void checkInputs(const cv::Mat& src, const cv::Mat& mask, const char* who) {
    if (src.empty() || src.type() != CV_32FC1)
        throw std::invalid_argument(std::string(who) + ": src must be a non-empty CV_32FC1 image");
    if (!mask.empty() &&
        (mask.type() != CV_8UC1 || mask.rows != src.rows || mask.cols != src.cols))
        throw std::invalid_argument(std::string(who) + ": mask must be CV_8UC1 with the size of src");
}

int toBin(float v) {
    if (!std::isfinite(v)) throw std::invalid_argument("threshold: image contains NaN or Inf");
    return static_cast<int>(std::clamp(v, 0.f, 1.f) * (kBins - 1) + 0.5f);
}

}  // namespace

// Otsu (1979): choose the threshold that maximizes the between-class variance
//   sigma_B^2(t) = w0(t) * w1(t) * (mu0(t) - mu1(t))^2
// over a 256-bin histogram of the pixels inside the mask.
float otsuThreshold(const cv::Mat& src, const cv::Mat& mask) {
    checkInputs(src, mask, "otsuThreshold");

    std::array<double, kBins> hist{};
    double total = 0.0;
    for (int y = 0; y < src.rows; ++y) {
        const float* s = src.ptr<float>(y);
        const uchar* m = mask.empty() ? nullptr : mask.ptr<uchar>(y);
        for (int x = 0; x < src.cols; ++x) {
            if (m && m[x] == 0) continue;
            hist[toBin(s[x])] += 1.0;
            total += 1.0;
        }
    }
    if (total == 0.0) throw std::invalid_argument("otsuThreshold: mask selects no pixels");

    double sumAll = 0.0;
    for (int i = 0; i < kBins; ++i) sumAll += i * hist[i];

    double w0 = 0.0, sum0 = 0.0, bestVar = -1.0;
    int bestT = -1;
    for (int t = 0; t < kBins - 1; ++t) {
        w0 += hist[t];
        sum0 += t * hist[t];
        const double w1 = total - w0;
        if (w0 == 0.0 || w1 == 0.0) continue;
        const double mu0 = sum0 / w0;
        const double mu1 = (sumAll - sum0) / w1;
        const double var = w0 * w1 * (mu0 - mu1) * (mu0 - mu1);
        if (var > bestVar) {
            bestVar = var;
            bestT = t;
        }
    }

    // Only one brightness level present: there is nothing to split, so return
    // 1.0 (binarize then selects no pixel, since values are at most 1).
    if (bestT < 0) return 1.f;

    // Bins 0..bestT are background; this value separates bin bestT from bestT+1.
    return (bestT + 0.5f) / (kBins - 1);
}

cv::Mat binarize(const cv::Mat& src, float t, const cv::Mat& mask) {
    checkInputs(src, mask, "binarize");
    cv::Mat out(src.rows, src.cols, CV_8UC1);
    for (int y = 0; y < src.rows; ++y) {
        const float* s = src.ptr<float>(y);
        const uchar* m = mask.empty() ? nullptr : mask.ptr<uchar>(y);
        uchar* d = out.ptr<uchar>(y);
        for (int x = 0; x < src.cols; ++x) {
            const bool inside = !m || m[x] != 0;
            d[x] = (inside && s[x] > t) ? 255 : 0;
        }
    }
    return out;
}

}  // namespace la::basic
