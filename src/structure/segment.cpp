// Owner: Ranju Kim
#include "structure/segment.h"

#include <stdexcept>
#include <vector>

#include "basic/filter.h"
#include "basic/morphology.h"
#include "basic/threshold.h"

namespace la::structure {

namespace {

constexpr float kBlurSigma = 1.5f;      // remove sensor noise / tiny bubbles before Otsu
constexpr int kRimMargin = 6;           // ignore the bright cup rim and its reflection
constexpr int kOpenRadius = 2;          // remove isolated bright specks
constexpr int kCloseRadius = 3;         // fill small holes inside the foam
constexpr float kMinBlobFraction = 0.003f;  // blobs smaller than 0.3% of the cup are noise
constexpr float kRimBand = 0.85f;       // outer band: distance > 85% of the radius
constexpr float kRimShare = 0.6f;       // blob with > 60% of its pixels in the band = cup rim

// If the cup circle is a few pixels off, part of the white ceramic rim falls
// inside the mask and looks like milk foam. Such a blob is a thin crescent
// that lies almost entirely in the outer band, whereas a latte art pattern
// that touches the edge still has most of its pixels further inside.
cv::Mat removeRimBlobs(const cv::Mat& bin, const NormalizedCup& cup) {
    std::vector<int> labels;
    const std::vector<int> sizes = basic::labelComponents(bin, labels);
    std::vector<int> inBand(sizes.size(), 0);
    const float band2 = (kRimBand * cup.radius) * (kRimBand * cup.radius);
    for (int y = 0; y < bin.rows; ++y)
        for (int x = 0; x < bin.cols; ++x) {
            const int l = labels[static_cast<size_t>(y) * bin.cols + x];
            if (l < 0) continue;
            const float dx = x + 0.5f - cup.center.x, dy = y + 0.5f - cup.center.y;
            if (dx * dx + dy * dy > band2) ++inBand[l];
        }

    cv::Mat out(bin.rows, bin.cols, CV_8UC1);
    for (int y = 0; y < bin.rows; ++y) {
        uchar* d = out.ptr<uchar>(y);
        for (int x = 0; x < bin.cols; ++x) {
            const int l = labels[static_cast<size_t>(y) * bin.cols + x];
            d[x] = (l >= 0 && inBand[l] <= kRimShare * sizes[l]) ? 255 : 0;
        }
    }
    return out;
}

}  // namespace

// Milk foam is brighter than crema, so the pattern is the bright class of an
// Otsu split computed only inside the cup. Separate blobs (e.g. the layers of
// a tulip) are all kept; only tiny specks are dropped.
PatternMask segmentPattern(const NormalizedCup& cup) {
    if (cup.gray.empty() || cup.cupMask.empty())
        throw std::invalid_argument("segmentPattern: cup.gray and cup.cupMask are required");

    const cv::Mat blurred = basic::gaussianBlur(cup.gray, kBlurSigma);
    const cv::Mat inner = basic::erode(cup.cupMask, kRimMargin);

    const float t = basic::otsuThreshold(blurred, inner);
    cv::Mat bin = basic::binarize(blurred, t, inner);
    bin = basic::open(bin, kOpenRadius);
    bin = basic::close(bin, kCloseRadius);

    double cupArea = 0.0;
    for (int y = 0; y < inner.rows; ++y) {
        const uchar* m = inner.ptr<uchar>(y);
        for (int x = 0; x < inner.cols; ++x) cupArea += m[x] != 0;
    }
    bin = basic::removeSmallComponents(bin, static_cast<int>(cupArea * kMinBlobFraction));
    bin = removeRimBlobs(bin, cup);

    PatternMask pm;
    pm.mask = bin;
    double sx = 0.0, sy = 0.0, n = 0.0;
    for (int y = 0; y < bin.rows; ++y) {
        const uchar* b = bin.ptr<uchar>(y);
        for (int x = 0; x < bin.cols; ++x) {
            if (b[x] == 0) continue;
            sx += x + 0.5;  // pixel centers, same convention as cupMask
            sy += y + 0.5;
            n += 1.0;
        }
    }
    pm.area = static_cast<float>(n);
    pm.centroid = n > 0 ? cv::Point2f(static_cast<float>(sx / n), static_cast<float>(sy / n))
                        : cup.center;
    return pm;
}

}  // namespace la::structure
