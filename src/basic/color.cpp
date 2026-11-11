#include "basic/color.h"

#include <algorithm>
#include <stdexcept>

#include "core/todo.h"

namespace la::basic {

// Reference implementation that shows the coding style: allocate the output
// Mat, then walk pixels with ptr<T>(row). No cv:: processing functions.
cv::Mat bgrToGray(const cv::Mat& bgr) {
    if (bgr.type() != CV_8UC3) throw std::invalid_argument("bgrToGray: expected CV_8UC3");
    cv::Mat gray(bgr.rows, bgr.cols, CV_32FC1);
    for (int y = 0; y < bgr.rows; ++y) {
        const uchar* s = bgr.ptr<uchar>(y);
        float* d = gray.ptr<float>(y);
        for (int x = 0; x < bgr.cols; ++x) {
            const uchar B = s[3 * x], G = s[3 * x + 1], R = s[3 * x + 2];
            d[x] = (0.114f * B + 0.587f * G + 0.299f * R) / 255.f;
        }
    }
    return gray;
}

void bgrToLab(const cv::Mat& bgr, cv::Mat& L, cv::Mat& a, cv::Mat& b) {
    (void)bgr; (void)L; (void)a; (void)b;
    LA_TODO("bgrToLab: sRGB -> linear -> XYZ -> Lab");
}

cv::Mat grayToBgr8(const cv::Mat& gray) {
    if (gray.type() != CV_32FC1) throw std::invalid_argument("grayToBgr8: expected CV_32FC1");
    cv::Mat out(gray.rows, gray.cols, CV_8UC3);
    for (int y = 0; y < gray.rows; ++y) {
        const float* s = gray.ptr<float>(y);
        uchar* d = out.ptr<uchar>(y);
        for (int x = 0; x < gray.cols; ++x) {
            const uchar v = static_cast<uchar>(std::clamp(s[x], 0.f, 1.f) * 255.f + 0.5f);
            d[3 * x] = d[3 * x + 1] = d[3 * x + 2] = v;
        }
    }
    return out;
}

}  // namespace la::basic
