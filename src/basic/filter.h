#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// Separable Gaussian blur on CV_32FC1. Border: replicate.
cv::Mat gaussianBlur(const cv::Mat& src, float sigma);

// Generic 2D convolution on CV_32FC1 with a CV_32FC1 kernel (odd size).
cv::Mat convolve2D(const cv::Mat& src, const cv::Mat& kernel);

// Contrast Limited Adaptive Histogram Equalization on CV_32FC1 0..1.
cv::Mat clahe(const cv::Mat& src, int tilesX = 8, int tilesY = 8, float clipLimit = 2.f);

}  // namespace la::basic
