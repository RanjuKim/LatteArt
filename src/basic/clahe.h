// Owner: Junkyu Kim
#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// Contrast Limited Adaptive Histogram Equalization on CV_32FC1 0..1.
cv::Mat clahe(const cv::Mat& src, int tilesX = 8, int tilesY = 8, float clipLimit = 2.f);

}  // namespace la::basic
