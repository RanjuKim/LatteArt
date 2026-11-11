// Owner: Jaeku Kang
#include "normalize/manual.h"

#include <cmath>
#include <stdexcept>

#include "basic/color.h"

namespace la::normalize {

cv::Mat canonicalCrop(const cv::Mat& bgr, float cx, float cy, float r) {
    if (bgr.type() != CV_8UC3) throw std::invalid_argument("canonicalCrop: expected CV_8UC3");
    if (r <= 0.f) throw std::invalid_argument("canonicalCrop: radius must be positive");

    const int N = kCanonicalSize;
    const float c = N / 2.f;
    const float s = r / kCanonicalRadius;  // source pixels per output pixel
    cv::Mat out(N, N, CV_8UC3);

    for (int v = 0; v < N; ++v) {
        uchar* d = out.ptr<uchar>(v);
        for (int u = 0; u < N; ++u) {
            const float x = cx + (u - c) * s;
            const float y = cy + (v - c) * s;
            const int x0 = static_cast<int>(std::floor(x));
            const int y0 = static_cast<int>(std::floor(y));
            if (x0 < 0 || y0 < 0 || x0 + 1 >= bgr.cols || y0 + 1 >= bgr.rows) {
                d[3 * u] = d[3 * u + 1] = d[3 * u + 2] = 0;
                continue;
            }
            const float fx = x - x0, fy = y - y0;
            const uchar* p0 = bgr.ptr<uchar>(y0) + 3 * x0;
            const uchar* p1 = bgr.ptr<uchar>(y0 + 1) + 3 * x0;
            for (int ch = 0; ch < 3; ++ch) {
                const float top = p0[ch] * (1 - fx) + p0[3 + ch] * fx;
                const float bot = p1[ch] * (1 - fx) + p1[3 + ch] * fx;
                d[3 * u + ch] = static_cast<uchar>(top * (1 - fy) + bot * fy + 0.5f);
            }
        }
    }
    return out;
}

NormalizedCup fromCanonicalImage(const cv::Mat& bgr512) {
    if (bgr512.type() != CV_8UC3 || bgr512.rows != kCanonicalSize || bgr512.cols != kCanonicalSize)
        throw std::invalid_argument("fromCanonicalImage: expected 512x512 CV_8UC3 (use --canon)");

    NormalizedCup cup;
    cup.bgr = bgr512.clone();
    cup.gray = basic::bgrToGray(bgr512);
    try {
        basic::bgrToLab(bgr512, cup.L, cup.a, cup.b);
    } catch (const std::logic_error&) {
        // bgrToLab not implemented yet: leave L/a/b empty.
    }

    cup.cupMask = cv::Mat(kCanonicalSize, kCanonicalSize, CV_8UC1);
    const float r2 = kCanonicalRadius * kCanonicalRadius;
    for (int y = 0; y < kCanonicalSize; ++y) {
        uchar* m = cup.cupMask.ptr<uchar>(y);
        for (int x = 0; x < kCanonicalSize; ++x) {
            const float dx = x + 0.5f - cup.center.x, dy = y + 0.5f - cup.center.y;
            m[x] = (dx * dx + dy * dy <= r2) ? 255 : 0;
        }
    }
    cup.ok = true;
    return cup;
}

}  // namespace la::normalize
