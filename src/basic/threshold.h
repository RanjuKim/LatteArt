// Owner: Ranju Kim
#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// Otsu threshold of CV_32FC1 0..1, using only pixels where mask != 0
// (pass an empty Mat to use all pixels). Returns the threshold in 0..1, or
// 1.0 when all pixels share one brightness level (nothing to split).
// Note: Otsu always splits the pixels in two, even when there is no real
// object; check the contrast between the two classes when that matters.
// Throws on an empty selection or NaN / Inf pixels.
float otsuThreshold(const cv::Mat& src, const cv::Mat& mask = cv::Mat());

// src > t -> 255 else 0, restricted to mask. Returns CV_8UC1.
cv::Mat binarize(const cv::Mat& src, float t, const cv::Mat& mask = cv::Mat());

}  // namespace la::basic
