#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// Binary morphology on CV_8UC1 (0/255) with a disk structuring element.
cv::Mat erode(const cv::Mat& bin, int radius);
cv::Mat dilate(const cv::Mat& bin, int radius);
cv::Mat open(const cv::Mat& bin, int radius);   // erode -> dilate
cv::Mat close(const cv::Mat& bin, int radius);  // dilate -> erode

// Keep only the largest 8-connected component.
cv::Mat largestComponent(const cv::Mat& bin);

}  // namespace la::basic
