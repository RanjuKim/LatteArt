// Owner: SHARED - change only with team agreement (PR approved by all)
#include "io/io.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace la::io {

cv::Mat loadColor(const std::string& path) {
    cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
    if (img.empty()) throw std::runtime_error("cannot read image: " + path);
    return img;
}

void save(const std::string& path, const cv::Mat& img) {
    if (!cv::imwrite(path, img)) throw std::runtime_error("cannot write image: " + path);
}

void show(const std::string& window, const cv::Mat& img) { cv::imshow(window, img); }

int waitKey(int ms) { return cv::waitKey(ms); }

}  // namespace la::io
