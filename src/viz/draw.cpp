// Owner: Junkyu Kim
#include "viz/draw.h"

#include "core/todo.h"

namespace la::viz {

void setPixel(cv::Mat& img, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= img.cols || y >= img.rows) return;
    uchar* p = img.ptr<uchar>(y) + 3 * x;
    p[0] = c[0];
    p[1] = c[1];
    p[2] = c[2];
}

void drawLine(cv::Mat& img, cv::Point a, cv::Point b, Color c, int thickness) {
    (void)img; (void)a; (void)b; (void)c; (void)thickness;
    LA_TODO("drawLine: Bresenham");
}

void drawCircle(cv::Mat& img, cv::Point center, int radius, Color c, int thickness) {
    (void)img; (void)center; (void)radius; (void)c; (void)thickness;
    LA_TODO("drawCircle: midpoint circle");
}

void drawCross(cv::Mat& img, cv::Point center, int size, Color c) {
    (void)img; (void)center; (void)size; (void)c;
    LA_TODO("drawCross");
}

void drawRect(cv::Mat& img, cv::Point topLeft, cv::Point bottomRight, Color c, int thickness) {
    (void)img; (void)topLeft; (void)bottomRight; (void)c; (void)thickness;
    LA_TODO("drawRect");
}

void drawText(cv::Mat& img, cv::Point origin, const std::string& text, Color c, int scale) {
    (void)img; (void)origin; (void)text; (void)c; (void)scale;
    LA_TODO("drawText: 5x7 bitmap font table");
}

cv::Mat sideBySide(const cv::Mat& a, const cv::Mat& b, int gap) {
    (void)a; (void)b; (void)gap;
    LA_TODO("sideBySide");
}

}  // namespace la::viz
