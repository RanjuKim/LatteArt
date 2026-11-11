#include "basic/threshold.h"

#include "core/todo.h"

namespace la::basic {

float otsuThreshold(const cv::Mat& src, const cv::Mat& mask) {
    (void)src; (void)mask;
    LA_TODO("otsuThreshold: 256-bin histogram, maximize between-class variance");
}

cv::Mat binarize(const cv::Mat& src, float t, const cv::Mat& mask) {
    (void)src; (void)t; (void)mask;
    LA_TODO("binarize");
}

}  // namespace la::basic
