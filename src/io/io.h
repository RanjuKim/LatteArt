// The ONLY place allowed to call OpenCV I/O functions.
#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace la::io {

cv::Mat loadColor(const std::string& path);  // CV_8UC3, throws if missing
void save(const std::string& path, const cv::Mat& img);
void show(const std::string& window, const cv::Mat& img);
int waitKey(int ms = 0);

}  // namespace la::io
