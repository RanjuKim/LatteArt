// Owner: Jaeku Kang
#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// Sobel gradients of CV_32FC1 -> gx, gy (CV_32FC1).
void sobel(const cv::Mat& src, cv::Mat& gx, cv::Mat& gy);

// Canny: Gaussian -> Sobel -> non-maximum suppression -> double threshold
// with hysteresis. Returns CV_8UC1 edge map (255 = edge).
cv::Mat canny(const cv::Mat& src, float lowThresh, float highThresh, float sigma = 1.4f);

}  // namespace la::basic
