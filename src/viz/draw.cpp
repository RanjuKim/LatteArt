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

#include <cstdlib>
#include <cctype>
#include <string>

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
    if (thickness <= 0) return;
    for (int offset = 0; offset < thickness; ++offset) {
        const int x1 = topLeft.x + offset;
        const int y1 = topLeft.y + offset;
        const int x2 = bottomRight.x - offset;
        const int y2 = bottomRight.y - offset;
        if (x1 > x2 || y1 > y2) break;

        for (int x = x1; x <= x2; ++x) {
            setPixel(img, x, y1, c);
            setPixel(img, x, y2, c);
        }
        for (int y = y1; y <= y2; ++y) {
            setPixel(img, x1, y, c);
            setPixel(img, x2, y, c);
        }
    }
}

// Step 1-b: "+" marker used to show the cup / pattern center.
// Horizontal bar: y = center.y, x from center.x - size to center.x + size
// Vertical bar:   x = center.x, y from center.y - size to center.y + size
void drawCross(cv::Mat& img, cv::Point center, int size, Color c) {
    if (size < 0) return;
    for (int x = center.x - size; x <= center.x + size; ++x)
        setPixel(img, x, center.y, c);
    for (int y = center.y - size; y <= center.y + size; ++y)
        setPixel(img, center.x, y, c);
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
    if (a.empty() || b.empty())
        return cv::Mat();

    if (gap < 0) gap = 0;
    const int height = a.rows > b.rows ? a.rows : b.rows;
    const int width = a.cols + gap + b.cols;
    cv::Mat out(height, width, CV_8UC3);

    for (int y = 0; y < height; ++y) {
        uchar* dst = out.ptr<uchar>(y);
        for (int x = 0; x < width; ++x)
            dst[3 * x] = dst[3 * x + 1] = dst[3 * x + 2] = 0;
    }

    for (int y = 0; y < a.rows; ++y) {
        const uchar* src = a.ptr<uchar>(y);
        uchar* dst = out.ptr<uchar>(y);
        for (int x = 0; x < a.cols; ++x) {
            dst[3 * x] = src[3 * x];
            dst[3 * x + 1] = src[3 * x + 1];
            dst[3 * x + 2] = src[3 * x + 2];
        }
    }

    for (int y = 0; y < b.rows; ++y) {
        const uchar* src = b.ptr<uchar>(y);
        uchar* dst = out.ptr<uchar>(y);
        const int offset = a.cols + gap;
        for (int x = 0; x < b.cols; ++x) {
            dst[3 * (offset + x)] = src[3 * x];
            dst[3 * (offset + x) + 1] = src[3 * x + 1];
            dst[3 * (offset + x) + 2] = src[3 * x + 2];
        }
    }
    return out;
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
    if (scale <= 0) return;

    struct Glyph {
        char ch;
        unsigned char rows[7];
    };
    static const Glyph font[] = {
        {'0', {14,17,19,21,25,17,14}}, {'1', {4,12,4,4,4,4,14}},
        {'2', {14,17,1,2,4,8,31}}, {'3', {30,1,1,14,1,1,30}},
        {'4', {2,6,10,18,31,2,2}}, {'5', {31,16,16,30,1,1,30}},
        {'6', {14,16,16,30,17,17,14}}, {'7', {31,1,2,4,8,8,8}},
        {'8', {14,17,17,14,17,17,14}}, {'9', {14,17,17,15,1,1,14}},
        {'A', {14,17,17,31,17,17,17}}, {'B', {30,17,17,30,17,17,30}},
        {'C', {14,17,16,16,16,17,14}}, {'D', {30,17,17,17,17,17,30}},
        {'E', {31,16,16,30,16,16,31}}, {'F', {31,16,16,30,16,16,16}},
        {'G', {14,17,16,23,17,17,15}}, {'H', {17,17,17,31,17,17,17}},
        {'I', {14,4,4,4,4,4,14}}, {'J', {7,2,2,2,2,18,12}},
        {'K', {17,18,20,24,20,18,17}}, {'L', {16,16,16,16,16,16,31}},
        {'M', {17,27,21,21,17,17,17}}, {'N', {17,25,21,19,17,17,17}},
        {'O', {14,17,17,17,17,17,14}}, {'P', {30,17,17,30,16,16,16}},
        {'Q', {14,17,17,17,21,18,13}}, {'R', {30,17,17,30,20,18,17}},
        {'S', {15,16,16,14,1,1,30}}, {'T', {31,4,4,4,4,4,4}},
        {'U', {17,17,17,17,17,17,14}}, {'V', {17,17,17,17,17,10,4}},
        {'W', {17,17,17,21,21,21,10}}, {'X', {17,17,10,4,10,17,17}},
        {'Y', {17,17,10,4,4,4,4}}, {'Z', {31,2,4,8,16,16,31}},
        {' ', {0,0,0,0,0,0,0}}, {'.', {0,0,0,0,0,12,12}},
        {':', {0,12,12,0,12,12,0}}, {'/', {1,2,2,4,8,8,16}},
        {'%', {18,18,4,8,16,18,18}}, {'-', {0,0,0,31,0,0,0}}
    };

    for (std::size_t i = 0; i < text.size(); ++i) {
        char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
        const Glyph* glyph = nullptr;
        for (const Glyph& candidate : font) {
            if (candidate.ch == ch) {
                glyph = &candidate;
                break;
            }
        }
        if (!glyph) continue;

        const int left = origin.x + static_cast<int>(i) * 6 * scale;
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((glyph->rows[row] & (1 << (4 - col))) != 0) {
                    for (int dy = 0; dy < scale; ++dy) {
                        for (int dx = 0; dx < scale; ++dx) {
                            setPixel(img, left + col * scale + dx,
                                     origin.y + row * scale + dy, c);
                        }
                    }
                }
            }
        }
    }
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
    if (thickness <= 0) return;
    int x = a.x, y = a.y;
    const int dx = std::abs(b.x - a.x);
    const int dy = -std::abs(b.y - a.y);
    const int sx = a.x < b.x ? 1 : -1;
    const int sy = a.y < b.y ? 1 : -1;
    int err = dx + dy;
    const int half = thickness / 2;

    for (;;) {
        for (int oy = -half; oy < half + (thickness % 2 == 0 ? 0 : 1); ++oy) {
            for (int ox = -half; ox < half + (thickness % 2 == 0 ? 0 : 1); ++ox)
                setPixel(img, x + ox, y + oy, c);
        }
        if (x == b.x && y == b.y) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
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
    if (radius < 0 || thickness <= 0) return;
    for (int r = radius; r > radius - thickness && r >= 0; --r) {
        int x = r, y = 0;
        int err = 1 - r;
        while (x >= y) {
            setPixel(img, center.x + x, center.y + y, c);
            setPixel(img, center.x - x, center.y + y, c);
            setPixel(img, center.x + x, center.y - y, c);
            setPixel(img, center.x - x, center.y - y, c);
            setPixel(img, center.x + y, center.y + x, c);
            setPixel(img, center.x - y, center.y + x, c);
            setPixel(img, center.x + y, center.y - x, c);
            setPixel(img, center.x - y, center.y - x, c);

            ++y;
            if (err < 0) {
                err += 2 * y + 1;
            } else {
                --x;
                err += 2 * (y - x) + 1;
            }
        }
    }
}

}  // namespace la::viz
