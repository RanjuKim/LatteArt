#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// Otsu threshold of CV_32FC1 0..1, using only pixels where mask != 0
// (pass an empty Mat to use all pixels). Returns the threshold in 0..1.
float otsuThreshold(const cv::Mat& src, const cv::Mat& mask = cv::Mat());

// src > t -> 255 else 0, restricted to mask. Returns CV_8UC1.
cv::Mat binarize(const cv::Mat& src, float t, const cv::Mat& mask = cv::Mat());

}  // namespace la::basic
