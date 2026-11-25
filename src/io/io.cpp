// Owner: SHARED - change only with team agreement (PR approved by all)
#include "io/io.h"

#include <fstream>
#include <iterator>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>
#include <vector>

namespace la::io {

cv::Mat loadColorFile(const std::filesystem::path& path) {
    // std::ifstream takes the wide path on Windows, so Korean folders work.
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read image: " + path.u8string());
    const std::vector<uchar> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.empty()) throw std::runtime_error("cannot read image: " + path.u8string());
    cv::Mat img = cv::imdecode(bytes, cv::IMREAD_COLOR);
    if (img.empty()) throw std::runtime_error("cannot decode image: " + path.u8string());
    return img;
}

cv::Mat loadColor(const std::string& path) {
    cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
    if (!img.empty()) return img;
    // imread cannot open non-ASCII paths. The narrow string uses the system
    // code page (e.g. a Korean path typed on the command line), which
    // std::filesystem::path converts correctly.
    std::error_code ec;
    const std::filesystem::path p(path);
    if (std::filesystem::exists(p, ec)) return loadColorFile(p);
    throw std::runtime_error("cannot read image: " + path);
}

void save(const std::string& path, const cv::Mat& img) {
    if (!cv::imwrite(path, img)) throw std::runtime_error("cannot write image: " + path);
}

void show(const std::string& window, const cv::Mat& img) { cv::imshow(window, img); }

int waitKey(int ms) { return cv::waitKey(ms); }

}  // namespace la::io
