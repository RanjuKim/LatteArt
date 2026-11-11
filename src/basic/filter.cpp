// Owner: Junkyu Kim
#include "basic/filter.h"

#include "core/todo.h"

namespace la::basic {

cv::Mat gaussianBlur(const cv::Mat& src, float sigma) {
    (void)src; (void)sigma;
    LA_TODO("gaussianBlur: build 1D kernel (radius ~3*sigma), convolve rows then columns");
}

cv::Mat convolve2D(const cv::Mat& src, const cv::Mat& kernel) {
    (void)src; (void)kernel;
    LA_TODO("convolve2D");
}

}  // namespace la::basic
