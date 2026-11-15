// Owner: Ranju Kim
#include "basic/morphology.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace la::basic {

namespace {

void checkBinary(const cv::Mat& bin, const char* who) {
    if (bin.empty() || bin.type() != CV_8UC1)
        throw std::invalid_argument(std::string(who) + ": expected a non-empty CV_8UC1 image");
}

// Offsets (dx, dy) inside a disk of the given radius.
std::vector<std::pair<int, int>> diskOffsets(int radius) {
    std::vector<std::pair<int, int>> offs;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx)
            if (dx * dx + dy * dy <= radius * radius) offs.emplace_back(dx, dy);
    return offs;
}

// Erosion: a pixel stays 255 only if every in-image pixel of the disk is 255.
// Dilation: a pixel becomes 255 if any in-image pixel of the disk is 255.
cv::Mat morph(const cv::Mat& bin, int radius, bool isErode, const char* who) {
    checkBinary(bin, who);
    if (radius < 0) throw std::invalid_argument(std::string(who) + ": radius must be >= 0");
    if (radius == 0) return bin.clone();

    const auto offs = diskOffsets(radius);
    cv::Mat out(bin.rows, bin.cols, CV_8UC1);
    for (int y = 0; y < bin.rows; ++y) {
        uchar* d = out.ptr<uchar>(y);
        for (int x = 0; x < bin.cols; ++x) {
            bool result = isErode;  // erode: assume 255 until a 0 is found
            for (const auto& [dx, dy] : offs) {
                const int sx = x + dx, sy = y + dy;
                if (sx < 0 || sy < 0 || sx >= bin.cols || sy >= bin.rows) continue;
                const bool on = bin.ptr<uchar>(sy)[sx] != 0;
                if (isErode && !on) { result = false; break; }
                if (!isErode && on) { result = true; break; }
            }
            d[x] = result ? 255 : 0;
        }
    }
    return out;
}

cv::Mat keepLabels(const cv::Mat& bin, const std::vector<int>& labels,
                   const std::vector<bool>& keep) {
    cv::Mat out(bin.rows, bin.cols, CV_8UC1);
    for (int y = 0; y < bin.rows; ++y) {
        uchar* d = out.ptr<uchar>(y);
        for (int x = 0; x < bin.cols; ++x) {
            const int l = labels[static_cast<size_t>(y) * bin.cols + x];
            d[x] = (l >= 0 && keep[l]) ? 255 : 0;
        }
    }
    return out;
}

}  // namespace

// 8-connected labeling with an explicit BFS queue.
std::vector<int> labelComponents(const cv::Mat& bin, std::vector<int>& labels) {
    checkBinary(bin, "labelComponents");
    const int W = bin.cols, H = bin.rows;
    labels.assign(static_cast<size_t>(W) * H, -1);
    std::vector<int> sizes;
    std::vector<int> queue;
    queue.reserve(static_cast<size_t>(W) * H);

    for (int y0 = 0; y0 < H; ++y0) {
        for (int x0 = 0; x0 < W; ++x0) {
            const size_t start = static_cast<size_t>(y0) * W + x0;
            if (bin.ptr<uchar>(y0)[x0] == 0 || labels[start] != -1) continue;

            const int id = static_cast<int>(sizes.size());
            int count = 0;
            queue.clear();
            queue.push_back(static_cast<int>(start));
            labels[start] = id;
            for (size_t head = 0; head < queue.size(); ++head) {
                const int p = queue[head], px = p % W, py = p / W;
                ++count;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = px + dx, ny = py + dy;
                        if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                        const size_t q = static_cast<size_t>(ny) * W + nx;
                        if (labels[q] != -1 || bin.ptr<uchar>(ny)[nx] == 0) continue;
                        labels[q] = id;
                        queue.push_back(static_cast<int>(q));
                    }
                }
            }
            sizes.push_back(count);
        }
    }
    return sizes;
}

cv::Mat erode(const cv::Mat& bin, int radius) { return morph(bin, radius, true, "erode"); }

cv::Mat dilate(const cv::Mat& bin, int radius) { return morph(bin, radius, false, "dilate"); }

cv::Mat open(const cv::Mat& bin, int radius) { return dilate(erode(bin, radius), radius); }

cv::Mat close(const cv::Mat& bin, int radius) { return erode(dilate(bin, radius), radius); }

cv::Mat largestComponent(const cv::Mat& bin) {
    checkBinary(bin, "largestComponent");
    std::vector<int> labels;
    const std::vector<int> sizes = labelComponents(bin, labels);
    std::vector<bool> keep(sizes.size(), false);
    int best = -1;
    for (int i = 0; i < static_cast<int>(sizes.size()); ++i)
        if (best < 0 || sizes[i] > sizes[best]) best = i;
    if (best >= 0) keep[best] = true;
    return keepLabels(bin, labels, keep);
}

cv::Mat removeSmallComponents(const cv::Mat& bin, int minArea) {
    checkBinary(bin, "removeSmallComponents");
    std::vector<int> labels;
    const std::vector<int> sizes = labelComponents(bin, labels);
    std::vector<bool> keep(sizes.size());
    for (size_t i = 0; i < sizes.size(); ++i) keep[i] = sizes[i] >= minArea;
    return keepLabels(bin, labels, keep);
}

}  // namespace la::basic
