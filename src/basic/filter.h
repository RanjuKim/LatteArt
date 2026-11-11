// Owner: Junkyu Kim  (priority: gaussianBlur is needed by Jaeku and Ranju by Nov 13)
#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// Separable Gaussian blur on CV_32FC1. Border: replicate.
cv::Mat gaussianBlur(const cv::Mat& src, float sigma);

// Generic 2D convolution on CV_32FC1 with a CV_32FC1 kernel (odd size).
// Also used by Ranju for the LoG kernel in texture.cpp.
cv::Mat convolve2D(const cv::Mat& src, const cv::Mat& kernel);

}  // namespace la::basic
