// Owner: Jaeku Kang
#pragma once
#include <array>
#include <opencv2/core.hpp>

namespace la::basic {

using Mat3 = std::array<double, 9>;  // row-major 3x3

// Bilinear sample of CV_32FC1 or CV_8UC3 at (x, y); outside -> 0.
// For each output pixel p, source = H * p (H maps DST -> SRC).
cv::Mat warpPerspective(const cv::Mat& src, const Mat3& dstToSrc, cv::Size dstSize);

Mat3 identity();
Mat3 multiply(const Mat3& A, const Mat3& B);
Mat3 inverse(const Mat3& A);
Mat3 rotationAbout(cv::Point2f center, double angleDeg);
Mat3 scaleAbout(cv::Point2f center, double sx, double sy);
Mat3 translation(double tx, double ty);

}  // namespace la::basic
