// Owner: Jaeku Kang
// Photo -> NormalizedCup (cup rim found, tilt corrected, scaled to the
// canonical frame, pattern axis rotated to vertical).
#pragma once
#include "core/types.h"

namespace la::normalize {

struct Ellipse {
    cv::Point2f center;
    float a = 0.f, b = 0.f;  // semi-major / semi-minor axes
    float angleDeg = 0.f;    // major-axis angle
};

// Edge points on the cup rim (Canny + radial filtering).
std::vector<cv::Point2f> findRimPoints(const cv::Mat& gray);

// Direct least-squares ellipse fit (Fitzgibbon et al., 1999).
Ellipse fitEllipse(const std::vector<cv::Point2f>& pts);

// Main axis of the pattern mask via 2x2 covariance eigenvectors (PCA).
float patternAxisAngleDeg(const cv::Mat& patternMask);

// Full normalization pipeline.
NormalizedCup normalizeCup(const cv::Mat& bgr);

}  // namespace la::normalize
