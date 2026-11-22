// Owner: SHARED - change only with team agreement (PR approved by all)
// Shared data contracts between modules.
// Need an extra field for your own module? Put a private struct in YOUR
// header instead of editing this file.
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
// Reference should be upright. Pipeline rotates the user to best match it.

struct PatternSpec {
    std::string name;      // "heart", "tulip", ...
    // Optional catalog metadata (e.g. a three-tier tulip), NOT a scoring target.
    // Legacy CLI/batch still use it to label/select the reference photo.
    int targetLayers = 0;
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

// Shared multi-line structure contract. Coordinates refer to the pattern after
// bounding-box centering and independent width/height normalization, not the cup.
// Sampling direction is independent of the quality/symmetry axis.
enum class ScanDirection { Vertical, Horizontal };
inline const char* directionName(ScanDirection direction) {
    return direction == ScanDirection::Vertical ? "vertical" : "horizontal";
}
struct Peak {
    float position = 0; // fraction along scan: top->bottom or left->right
    float width = 0;    // FWHM, fraction of pattern extent along scan
};

struct PeakMatch {
    int reference = -1, user = -1;
};

struct LineComparison {
    ScanDirection direction = ScanDirection::Vertical;
    float coordinate = 0; // vertical: fixed x / width; horizontal: fixed y / height
    // Ordered along the scan direction; size() is the peak count on this line.
    std::vector<Peak> reference, user;
    // Only successful pairs, in order, with valid indices into the above vectors.
    // Unmatched peaks are absent from matches; each peak is matched at most once.
    std::vector<PeakMatch> matches;
    // reference.size() == matches.size() + missing; likewise user and extra.
    int missing = 0, extra = 0;
    float spacingScore = 0, thicknessScore = 0, U = 0; // 0..1
    float bandLow = 0, bandHigh = 0; // this line's score bounds, NOT image bounds
};

struct StructureComparison {
    // Vertical then horizontal, each with increasing coordinate. Includes support on either image;
    // a selected line may have no detectable peaks. No image-wide layer count.
    std::vector<LineComparison> lines;
    float angleDeg = 0; // clockwise rotation applied to the user image
    float alignmentScore = 0; // structural search score (0..100), excludes Q
    // Bounding-box dimensions in canonical-cup pixels, after rotation but BEFORE
    // independent pattern scaling. Differences are abs(user/reference - 1).
    float referenceWidth = 0, referenceHeight = 0, userWidth = 0, userHeight = 0;
    float widthDifference = 0, heightDifference = 0;
    bool sizeOk = false; // false means final scoring must not proceed
    std::string sizeMessage;
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

struct DirectionScore {
    size_t lineCount = 0;
    float bandLow = 0, bandHigh = 0, U = 0;
    float alignmentScore = 0; // mean L + (H-L)*U, used for rotation search
    float finalScore = 0; // mean L + (H-L)*(alpha*U + (1-alpha)*Q); scoring sets alpha
};

struct ScoreResult {
    DirectionScore vertical, horizontal;
    // Direction means receive equal weight regardless of line counts.
    // Bounds can be fractional; they are not a single count-error band.
    float bandLow = 0.f, bandHigh = 0.f;
    float U = 0.f, Q = 0.f;
    float finalScore = 0.f;
    bool needsReview = false;
};

}  // namespace la
