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

// "Is there any foam at all?" Otsu always splits the cup in two, even a cup
// of plain crema (light and dark crema). The bright class only counts as milk
// foam if it is bright enough and clearly brighter than the dark class.
// Draft values on the 0..1 gray scale: foam is usually ~0.75-0.95, crema
// ~0.2-0.5. Re-check both with real photos.
constexpr float kMinFoamGray = 0.55f;
constexpr float kMinFoamContrast = 0.15f;

// Mean of `img` over the pixels where `sel` != 0 (and `inside` != 0).
// Returns -1 if no pixel is selected.
float meanWhere(const cv::Mat& img, const cv::Mat& inside, const cv::Mat& sel, bool selected) {
    double sum = 0.0, n = 0.0;
    for (int y = 0; y < img.rows; ++y) {
        const float* v = img.ptr<float>(y);
        const uchar* in = inside.ptr<uchar>(y);
        const uchar* s = sel.ptr<uchar>(y);
        for (int x = 0; x < img.cols; ++x) {
            if (!in[x] || (s[x] != 0) != selected) continue;
            sum += v[x];
            n += 1.0;
        }
    }
    return n > 0 ? static_cast<float>(sum / n) : -1.f;
}

// If the cup circle is a few pixels off, part of the white ceramic rim falls
// inside the mask and looks like milk foam. Such a blob is a thin crescent
// that lies almost entirely in the outer band, whereas a latte art pattern
// that touches the edge still has most of its pixels further inside.
// Known limits: a rim crescent that touches the pattern merges with it and is
// kept; a small real blob lying mostly near the edge is dropped. Accurate
// normalization (radius 240 = liquid edge, not the outer ceramic rim) matters
// more than this safety net.
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
// a tulip) are all kept; only tiny specks are dropped. A cup without foam
// gives an empty mask (area 0, centroid = cup center).
PatternMask segmentPattern(const NormalizedCup& cup) {
    if (cup.gray.empty() || cup.cupMask.empty())
        throw std::invalid_argument("segmentPattern: cup.gray and cup.cupMask are required");

    const cv::Mat blurred = basic::gaussianBlur(cup.gray, kBlurSigma);
    const cv::Mat inner = basic::erode(cup.cupMask, kRimMargin);

    const float t = basic::otsuThreshold(blurred, inner);
    cv::Mat bin = basic::binarize(blurred, t, inner);

    const float foam = meanWhere(blurred, inner, bin, true);
    const float crema = meanWhere(blurred, inner, bin, false);
    const bool hasFoam = foam >= kMinFoamGray && crema >= 0.f && foam - crema >= kMinFoamContrast;
    if (!hasFoam) {
        PatternMask none;
        none.mask = cv::Mat(cup.gray.rows, cup.gray.cols, CV_8UC1);
        for (int y = 0; y < none.mask.rows; ++y)
            for (int x = 0; x < none.mask.cols; ++x) none.mask.ptr<uchar>(y)[x] = 0;
        none.centroid = cup.center;
        return none;
    }

    bin = basic::open(bin, kOpenRadius);
    bin = basic::close(bin, kCloseRadius);
    for (int y = 0; y < bin.rows; ++y) {  // closing may grow past the inner mask
        uchar* b = bin.ptr<uchar>(y);
        const uchar* m = inner.ptr<uchar>(y);
        for (int x = 0; x < bin.cols; ++x)
            if (!m[x]) b[x] = 0;
    }

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
