// Owner: Jaeku Kang
// Photo -> canonical liquid surface; reference-relative rotation is selected
// by structure/layers, not by an ambiguous PCA direction.
#pragma once
#include "core/types.h"

namespace la::normalize {

struct Ellipse {
    cv::Point2f center;
    float a = 0.f, b = 0.f; // semi-major / semi-minor axes
    float angleDeg = 0.f;   // major-axis angle
};

// Edge points on the cup rim (Canny + radial filtering).
std::vector<cv::Point2f> findRimPoints(const cv::Mat &gray);

struct NormalizeParams {
    int detectionSize = 480;
    float cannyLow = 0.05f, cannyHigh = 0.15f;
    float minAxisRatio = 0.75f, minRadiusFraction = 0.12f;
    float minAngularCoverage = 0.60f, maxRelativeFitError = 0.05f;
    int glcmLevels = 16, glcmPatchRadius = 3;
    float textureWeight = 0.25f;
    float innerBoundarySupport = 0.85f; // minimum evidence relative to the strongest rim transition
    float liquidSearchMin = 0.78f, liquidSearchMax = 1.02f;
};
struct CupDetection {
    Ellipse rim, liquid; // input image coordinates
    float coverage = 0, fitError = 0;
};
CupDetection detectCup(const cv::Mat &bgr, const NormalizeParams &params = NormalizeParams());

// Normalized algebraic least-squares conic fit with ellipse validation.
// Not the generalized-eigensystem Fitzgibbon implementation.
Ellipse fitEllipse(const std::vector<cv::Point2f> &pts);

// Main axis of the pattern mask via 2x2 covariance eigenvectors (PCA).
float patternAxisAngleDeg(const cv::Mat &patternMask);

// Full normalization pipeline.
NormalizedCup normalizeCup(const cv::Mat &bgr);
NormalizedCup normalizeCup(const cv::Mat &bgr, const NormalizeParams &params);
NormalizedCup rotateCup(const NormalizedCup &cup, float angleDeg);

} // namespace la::normalize
