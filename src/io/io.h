// Owner: SHARED - change only with team agreement (PR approved by all)
// The ONLY place allowed to call OpenCV I/O functions.
#pragma once

#include <filesystem>
#include <opencv2/core.hpp>
#include <string>

namespace la::io {

// CV_8UC3, throws if missing. Tries cv::imread first; if that fails (e.g. a
// folder with Korean letters, which imread cannot open) it falls back to
// loadColorFile, so ASCII paths behave exactly as before.
cv::Mat loadColor(const std::string& path);

// Any path, including non-ASCII (Korean) folders: the file bytes are read with
// the C++ library and decoded with cv::imdecode (an OpenCV input function).
cv::Mat loadColorFile(const std::filesystem::path& path);
void save(const std::string& path, const cv::Mat& img);
void show(const std::string& window, const cv::Mat& img);
int waitKey(int ms = 0);

}  // namespace la::io
