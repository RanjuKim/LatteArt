// Owner: Jaeku Kang
#include "basic/edge.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#include "basic/filter.h"

namespace la::basic {

void sobel(const cv::Mat &src, cv::Mat &gx, cv::Mat &gy) {
    if (src.empty() || src.type() != CV_32FC1)
        throw std::invalid_argument("sobel: expected non-empty CV_32FC1");
    cv::Mat dx(src.rows, src.cols, CV_32FC1), dy(src.rows, src.cols, CV_32FC1);
    const int smooth[3] = {1, 2, 1};
    for (int y = 0; y < src.rows; ++y)
        for (int x = 0; x < src.cols; ++x) {
            float a = 0, b = 0;
            for (int j = -1; j <= 1; ++j)
                for (int i = -1; i <= 1; ++i) {
                    const float v = src.ptr<float>(
                        std::clamp(y + j, 0, src.rows - 1))[std::clamp(x + i, 0, src.cols - 1)];
                    a += v * i * smooth[j + 1];
                    b += v * j * smooth[i + 1];
                }
            dx.ptr<float>(y)[x] = a;
            dy.ptr<float>(y)[x] = b;
        }
    gx = dx;
    gy = dy;
}

cv::Mat canny(const cv::Mat &src, float lowThresh, float highThresh, float sigma) {
    if (!std::isfinite(lowThresh) || !std::isfinite(highThresh) || lowThresh <= 0 ||
        highThresh < lowThresh)
        throw std::invalid_argument("canny: require 0 < low <= high");
    cv::Mat gx, gy;
    sobel(gaussianBlur(src, sigma), gx, gy);
    const int w = src.cols, h = src.rows;
    std::vector<float> mag(size_t(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            mag[size_t(y) * w + x] = std::hypot(gx.ptr<float>(y)[x], gy.ptr<float>(y)[x]);
    std::vector<unsigned char> state(mag.size(), 0);
    std::vector<int> queue;
    for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) {
            float angle = std::atan2(gy.ptr<float>(y)[x], gx.ptr<float>(y)[x]) * 180.f /
                          3.14159265358979323846f;
            if (angle < 0)
                angle += 180;
            int dx = 1, dy = 0;
            if (angle >= 22.5f && angle < 67.5f)
                dy = 1;
            else if (angle >= 67.5f && angle < 112.5f) {
                dx = 0;
                dy = 1;
            } else if (angle >= 112.5f && angle < 157.5f)
                dy = -1;
            const int p = y * w + x;
            const float v = mag[p];
            if (v < lowThresh || v < mag[p + dy * w + dx] || v < mag[p - dy * w - dx])
                continue;
            state[p] = v >= highThresh ? 2 : 1;
            if (state[p] == 2)
                queue.push_back(p);
        }
    for (size_t k = 0; k < queue.size(); ++k) {
        int p = queue[k], x = p % w, y = p / w;
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i) {
                int xx = x + i, yy = y + j;
                if (xx < 0 || yy < 0 || xx >= w || yy >= h)
                    continue;
                int q = yy * w + xx;
                if (state[q] == 1) {
                    state[q] = 2;
                    queue.push_back(q);
                }
            }
    }
    cv::Mat out(h, w, CV_8UC1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            out.ptr<uchar>(y)[x] = state[y * w + x] == 2 ? 255 : 0;
    return out;
}

} // namespace la::basic
