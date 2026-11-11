#include "basic/morphology.h"

#include "core/todo.h"

namespace la::basic {

cv::Mat erode(const cv::Mat& bin, int radius) {
    (void)bin; (void)radius;
    LA_TODO("erode");
}

cv::Mat dilate(const cv::Mat& bin, int radius) {
    (void)bin; (void)radius;
    LA_TODO("dilate");
}

cv::Mat open(const cv::Mat& bin, int radius) { return dilate(erode(bin, radius), radius); }

cv::Mat close(const cv::Mat& bin, int radius) { return erode(dilate(bin, radius), radius); }

cv::Mat largestComponent(const cv::Mat& bin) {
    (void)bin;
    LA_TODO("largestComponent: BFS/union-find labeling");
}

}  // namespace la::basic
