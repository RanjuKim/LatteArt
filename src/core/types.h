// Shared data contracts between modules. Change only after team agreement.
//
// Library rule (assignment spec): only OpenCV I/O is allowed.
//   Allowed : cv::Mat as a container (constructor, create, clone, rows/cols,
//             type, ptr<T>, at<T>), cv::Point/Point2f/Size/Vec3b,
//             cv::imread / imwrite / imshow / waitKey (src/io only).
//   Forbidden: any cv:: processing function (cvtColor, GaussianBlur, resize,
//             threshold, Canny, findContours, ...) and Mat arithmetic
//             operators / convertTo / copyTo with mask.
// Run scripts/check_library_usage.ps1 before every merge.
#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace la {

// Canonical frame every normalized cup is mapped into.
constexpr int   kCanonicalSize   = 512;    // output image is 512 x 512
constexpr float kCanonicalRadius = 240.f;  // cup rim radius in that image
// Pattern main axis is rotated to point straight up (-y direction).

struct PatternSpec {
    std::string name;      // "heart", "tulip", ...
    int targetLayers = 0;  // e.g. 3 for a three-tier tulip
};

// Output of normalize/. Input of everything else.
struct NormalizedCup {
    cv::Mat bgr;      // CV_8UC3, kCanonicalSize^2 (for display)
    cv::Mat gray;     // CV_32FC1, 0..1
    cv::Mat L, a, b;  // CV_32FC1, CIE Lab (L 0..100)
    cv::Mat cupMask;  // CV_8UC1, 255 inside the cup rim
    cv::Point2f center{kCanonicalSize / 2.f, kCanonicalSize / 2.f};
    float radius = kCanonicalRadius;
    float axisAngleDeg = 0.f;  // rotation applied to make the axis vertical
    float tiltRatio = 1.f;     // ellipse minor/major before correction
    bool ok = false;
    std::string warning;       // e.g. "cup too tilted, please retake"
};

// Output of structure/segment.cpp
struct PatternMask {
    cv::Mat mask;  // CV_8UC1, 255 = milk foam pattern
    cv::Point2f centroid;
    float area = 0.f;  // pixels
};

// Output of structure/layers.cpp
struct LayerResult {
    int count = 0;                    // layers from peak detection
    int fftCount = 0;                 // layers from dominant frequency
    bool uncertain = false;           // count != fftCount -> human review
    std::vector<float> profile;       // averaged brightness along the axis
    std::vector<float> peakPos;       // peak positions (pixels along axis)
    std::vector<float> spacings;      // distances between adjacent peaks
    std::vector<float> fwhm;          // full width at half maximum per peak
    float spacingCV = 0.f;
    float thicknessCV = 0.f;
    float U = 0.f;                    // uniformity score 0..1
};

// One quality item: raw measurement + normalized score + explanation.
struct ItemScore {
    float raw = 0.f;       // user measurement
    float ref = 0.f;       // same measurement on the reference photo
    float score = 0.f;     // 0..1, higher is better
    std::string feedback;  // e.g. "pattern is 15% smaller than reference"
};

struct QualityResult {
    ItemScore contrast;  // 30%
    ItemScore shape;     // 25%
    ItemScore texture;   // 20%
    ItemScore position;  // 15%
    ItemScore size;      // 10%
    float Q = 0.f;       // weighted sum 0..1
};

struct ScoreResult {
    int deltaN = 0;
    float bandLow = 0.f, bandHigh = 0.f;
    float U = 0.f, Q = 0.f;
    float finalScore = 0.f;
    bool needsReview = false;
};

}  // namespace la
