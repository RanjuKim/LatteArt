#include "basic/edge.h"

#include "core/todo.h"

namespace la::basic {

void sobel(const cv::Mat& src, cv::Mat& gx, cv::Mat& gy) {
    (void)src; (void)gx; (void)gy;
    LA_TODO("sobel");
}

cv::Mat canny(const cv::Mat& src, float lowThresh, float highThresh, float sigma) {
    (void)src; (void)lowThresh; (void)highThresh; (void)sigma;
    LA_TODO("canny");
}

}  // namespace la::basic
