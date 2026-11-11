// Owner: Jaeku Kang
// Manual normalization: lets everyone build test data in the canonical
// frame BEFORE the automatic normalizeCup() is finished.
//
//   LatteArt.exe --canon photo.jpg <cx> <cy> <r> out.png
//   (cx, cy, r = cup rim center and radius in photo pixels; read them in
//    Paint or any viewer that shows cursor coordinates)
#pragma once
#include "core/types.h"

namespace la::normalize {

// Crops the circle (cx, cy, r) and resamples it (bilinear) into a
// kCanonicalSize x kCanonicalSize CV_8UC3 image with the rim at
// kCanonicalRadius. No tilt or rotation correction.
cv::Mat canonicalCrop(const cv::Mat& bgr, float cx, float cy, float r);

// Wraps an image that is ALREADY in the canonical frame (e.g. the output of
// canonicalCrop) into a NormalizedCup: fills bgr, gray, cupMask, and L/a/b
// once bgrToLab is implemented (left empty until then).
NormalizedCup fromCanonicalImage(const cv::Mat& bgr512);

}  // namespace la::normalize
