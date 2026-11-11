// Owner: Cecelia Hein
// Drawing functions for the result image. OpenCV drawing functions
// (cv::line, cv::circle, cv::putText, ...) are NOT allowed by the assignment,
// so we draw by setting pixels ourselves.
//
// All functions work on a color image (CV_8UC3, 3 bytes per pixel: B, G, R).
// Coordinates: x = column (left -> right), y = row (top -> bottom),
// (0, 0) is the top-left corner.
//
// Check your work with:  LatteArt.exe --debug viz <canon.png>
// It saves debug_viz.png (see src/debug/viz.cpp).
#pragma once
#include <opencv2/core.hpp>
#include <string>

namespace la::viz {

using Color = cv::Vec3b;  // B, G, R  (note: blue first)
inline const Color kRed{0, 0, 255}, kGreen{0, 200, 0}, kYellow{0, 220, 255}, kWhite{255, 255, 255};

// Colors one pixel. Does nothing if (x, y) is outside the image. (done)
void setPixel(cv::Mat& img, int x, int y, Color c);

// Step 1 (very easy)
void drawRect(cv::Mat& img, cv::Point topLeft, cv::Point bottomRight, Color c, int thickness = 1);
void drawCross(cv::Mat& img, cv::Point center, int size, Color c);

// Step 2 (easy)
cv::Mat sideBySide(const cv::Mat& a, const cv::Mat& b, int gap = 8);

// Step 3 (easy, mostly typing the font table)
void drawText(cv::Mat& img, cv::Point origin, const std::string& text, Color c, int scale = 2);

// Step 4 (medium - hand over to Junkyu if stuck)
void drawLine(cv::Mat& img, cv::Point a, cv::Point b, Color c, int thickness = 1);
void drawCircle(cv::Mat& img, cv::Point center, int radius, Color c, int thickness = 1);

}  // namespace la::viz
