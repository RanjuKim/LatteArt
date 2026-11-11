// Owner: Junkyu Kim
// Own drawing primitives for the result image (OpenCV drawing functions such
// as line / circle / putText are NOT allowed). All work on CV_8UC3.
#pragma once
#include <opencv2/core.hpp>
#include <string>

namespace la::viz {

using Color = cv::Vec3b;  // B, G, R
inline const Color kRed{0, 0, 255}, kGreen{0, 200, 0}, kYellow{0, 220, 255}, kWhite{255, 255, 255};

void setPixel(cv::Mat& img, int x, int y, Color c);  // ignores out-of-range
void drawLine(cv::Mat& img, cv::Point a, cv::Point b, Color c, int thickness = 1);  // Bresenham
void drawCircle(cv::Mat& img, cv::Point center, int radius, Color c, int thickness = 1);
void drawCross(cv::Mat& img, cv::Point center, int size, Color c);
void drawRect(cv::Mat& img, cv::Point topLeft, cv::Point bottomRight, Color c, int thickness = 1);

// Own 5x7 bitmap font (digits, A-Z, a few symbols), scaled by `scale`.
void drawText(cv::Mat& img, cv::Point origin, const std::string& text, Color c, int scale = 2);

// Places a and b side by side (heights padded to the larger one).
cv::Mat sideBySide(const cv::Mat& a, const cv::Mat& b, int gap = 8);

}  // namespace la::viz
