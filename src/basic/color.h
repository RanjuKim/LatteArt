#pragma once
#include <opencv2/core.hpp>

namespace la::basic {

// CV_8UC3 (BGR) -> CV_32FC1 gray in 0..1 (ITU-R BT.601 weights).
cv::Mat bgrToGray(const cv::Mat& bgr);

// CV_8UC3 (BGR) -> three CV_32FC1 planes, CIE Lab with D65 white.
void bgrToLab(const cv::Mat& bgr, cv::Mat& L, cv::Mat& a, cv::Mat& b);

// CV_32FC1 0..1 -> CV_8UC3 gray image (for display / saving).
cv::Mat grayToBgr8(const cv::Mat& gray);

}  // namespace la::basic
