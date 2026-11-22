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
    float arcCenterTolerance = 0.15f, arcAxisTolerance = 0.20f;
    int maxArcCandidates = 48;
    int ransacIterations = 300;
    float inlierTolerance = 0.025f, minInlierFraction = 0.60f;
};
// Diagnostic geometry/points use the resized detection image coordinates.
struct RimCandidateDiagnostic {
    Ellipse ellipse;
    std::vector<cv::Point2f> points, inliers;
    std::vector<int> components;
    std::string method, rejection;
    float coverage = 0, fitError = 0, inlierFraction = 0, score = 0;
    bool fitted = false, accepted = false;
};
struct CupDiagnostics {
    cv::Mat gray, edges;
    std::vector<RimCandidateDiagnostic> candidates;
    float detectionScale = 1;
};
struct CupDetection {
    Ellipse rim, liquid; // input image coordinates
    float coverage = 0, fitError = 0;
    std::string rimMethod;
    std::vector<int> rimComponents;
};
CupDetection detectCup(const cv::Mat &bgr, const NormalizeParams &params = NormalizeParams(),
                       CupDiagnostics *diagnostics = nullptr);

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
