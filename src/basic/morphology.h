// Owner: Ranju Kim
#pragma once
#include <opencv2/core.hpp>
#include <vector>

namespace la::basic {

// Binary morphology on CV_8UC1 (0/255) with a disk structuring element of
// the given radius (radius 0 returns a copy). Pixels outside the image are
// ignored, so borders are neither eroded nor grown artificially.
cv::Mat erode(const cv::Mat& bin, int radius);
cv::Mat dilate(const cv::Mat& bin, int radius);
cv::Mat open(const cv::Mat& bin, int radius);   // erode -> dilate (removes specks)
cv::Mat close(const cv::Mat& bin, int radius);  // dilate -> erode (fills small holes)

// 8-connected labeling. labels has rows*cols entries (row-major): -1 for
// background, otherwise the component index. Returns the size of each component.
std::vector<int> labelComponents(const cv::Mat& bin, std::vector<int>& labels);

// Keep only the largest 8-connected component.
cv::Mat largestComponent(const cv::Mat& bin);

// Keep every 8-connected component with at least minArea pixels.
// Used for latte art, where the layers of a tulip are separate blobs.
cv::Mat removeSmallComponents(const cv::Mat& bin, int minArea);

}  // namespace la::basic
