// Owner: Cecelia Hein
//
// How to work on this file
// ------------------------
// 1. Pick the next function below (they are in order of difficulty).
// 2. Delete the two lines "(void)...;" and "LA_TODO(...);" and write your code.
// 3. Build (Ctrl+Shift+B), then run:  LatteArt.exe --debug viz <canon.png>
// 4. Open debug_viz.png and check the result.
// 5. Only setPixel() is needed to draw anything: every function below is just
//    "decide which pixels to color, then call setPixel for each of them".
// Rules: do not call any cv:: function except cv::Mat(rows, cols, CV_8UC3),
// img.rows, img.cols and img.ptr<uchar>(row).
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

// ---------------------------------------------------------------------------
// Step 1-a: rectangle outline.
// Draw the 4 edges between topLeft (x1, y1) and bottomRight (x2, y2):
//   top    edge: y = y1, x from x1 to x2
//   bottom edge: y = y2, x from x1 to x2
//   left   edge: x = x1, y from y1 to y2
//   right  edge: x = x2, y from y1 to y2
// For thickness t, repeat with the rectangle shrunk by 1 pixel, t times.
void drawRect(cv::Mat& img, cv::Point topLeft, cv::Point bottomRight, Color c, int thickness) {
    (void)img; (void)topLeft; (void)bottomRight; (void)c; (void)thickness;
    LA_TODO("drawRect");
}

// Step 1-b: "+" marker used to show the cup / pattern center.
// Horizontal bar: y = center.y, x from center.x - size to center.x + size
// Vertical bar:   x = center.x, y from center.y - size to center.y + size
void drawCross(cv::Mat& img, cv::Point center, int size, Color c) {
    (void)img; (void)center; (void)size; (void)c;
    LA_TODO("drawCross");
}

// ---------------------------------------------------------------------------
// Step 2: put two images next to each other (reference cup | user cup).
//   width  = a.cols + gap + b.cols
//   height = the larger of a.rows and b.rows
//   1. cv::Mat out(height, width, CV_8UC3);  fill every pixel with black (0,0,0)
//   2. copy every pixel of a to the same (x, y) in out
//   3. copy every pixel of b to (x + a.cols + gap, y) in out
// Reading a pixel: const uchar* p = a.ptr<uchar>(y) + 3 * x;  // p[0]=B p[1]=G p[2]=R
cv::Mat sideBySide(const cv::Mat& a, const cv::Mat& b, int gap) {
    (void)a; (void)b; (void)gap;
    LA_TODO("sideBySide");
}

// ---------------------------------------------------------------------------
// Step 3: text with our own 5x7 pixel font.
// Each character is 7 rows of 5 pixels. Write each row as a 5-bit number,
// leftmost pixel = highest bit (16). Example for '1':
//      ..#..  -> 0b00100 = 4
//      .##..  -> 0b01100 = 12
//      ..#..  -> 4
//      ..#..  -> 4
//      ..#..  -> 4
//      ..#..  -> 4
//      .###.  -> 0b01110 = 14
//   so the table entry is  {'1', {4, 12, 4, 4, 4, 4, 14}}
//
// Needed characters: 0-9, A-Z, space, '.', ':', '/', '%', '-'
// (lower-case letters can be drawn as upper-case).
//
// Drawing: for each character at index i, its top-left corner is
//   (origin.x + i * 6 * scale, origin.y)          // 5 pixels + 1 gap
// for each row r (0..6) and column k (0..4), if bit (4 - k) of row r is 1,
// fill a scale x scale block starting at
//   (left + k * scale, origin.y + r * scale).
void drawText(cv::Mat& img, cv::Point origin, const std::string& text, Color c, int scale) {
    (void)img; (void)origin; (void)text; (void)c; (void)scale;
    LA_TODO("drawText: 5x7 bitmap font table");
}

// ---------------------------------------------------------------------------
// Step 4-a: straight line from a to b (Bresenham's algorithm).
//   dx = |b.x - a.x|, dy = -|b.y - a.y|
//   sx = (a.x < b.x) ? 1 : -1,  sy = (a.y < b.y) ? 1 : -1
//   err = dx + dy
//   loop:
//     setPixel(x, y)
//     if (x == b.x && y == b.y) stop
//     e2 = 2 * err
//     if (e2 >= dy) { err += dy; x += sx; }
//     if (e2 <= dx) { err += dx; y += sy; }
// For thickness > 1, draw a small filled square around each point instead.
void drawLine(cv::Mat& img, cv::Point a, cv::Point b, Color c, int thickness) {
    (void)img; (void)a; (void)b; (void)c; (void)thickness;
    LA_TODO("drawLine: Bresenham");
}

// Step 4-b: circle outline (midpoint circle algorithm).
//   x = radius, y = 0, err = 1 - radius
//   while (x >= y):
//     color the 8 symmetric points (cx +- x, cy +- y) and (cx +- y, cy +- x)
//     y += 1
//     if (err < 0) err += 2 * y + 1
//     else { x -= 1; err += 2 * (y - x) + 1; }
// For thickness t, draw circles with radius, radius-1, ..., radius-t+1.
void drawCircle(cv::Mat& img, cv::Point center, int radius, Color c, int thickness) {
    (void)img; (void)center; (void)radius; (void)c; (void)thickness;
    LA_TODO("drawCircle: midpoint circle");
}

}  // namespace la::viz
