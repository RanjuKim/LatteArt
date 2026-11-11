// Owner: Jaeku Kang
#include "normalize/normalize.h"

#include "core/todo.h"

namespace la::normalize {

std::vector<cv::Point2f> findRimPoints(const cv::Mat& gray) {
    (void)gray;
    LA_TODO("findRimPoints");
}

Ellipse fitEllipse(const std::vector<cv::Point2f>& pts) {
    (void)pts;
    LA_TODO("fitEllipse: Fitzgibbon direct least squares");
}

float patternAxisAngleDeg(const cv::Mat& patternMask) {
    (void)patternMask;
    LA_TODO("patternAxisAngleDeg: PCA of mask pixel coordinates");
}

NormalizedCup normalizeCup(const cv::Mat& bgr) {
    // Planned steps:
    //  1. gray = bgrToGray(bgr); blur; canny
    //  2. rim points -> fitEllipse; reject if b/a < 0.75 ("retake photo")
    //  3. map ellipse -> circle of kCanonicalRadius centered in kCanonicalSize
    //  4. rough pattern mask -> PCA axis -> rotate so axis points up
    //  5. fill gray, L/a/b, cupMask
    (void)bgr;
    LA_TODO("normalizeCup");
}

}  // namespace la::normalize
