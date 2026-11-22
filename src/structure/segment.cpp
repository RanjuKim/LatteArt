// Owner: Ranju Kim
#include "structure/segment.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
// foam if it is
//   1. brighter than the dark class: Michelson >= kMinMichelson,
//   2. whiter (less saturated) than it: chroma ratio <= kMaxChromaRatio.
//      Brighter crema (lighting, golden crema around dark spots) keeps its
//      brown color, white milk does not; this is the main check.
//   3. separated by an edge, not a slow lighting change: edge width <= kMaxEdgeWidth.
// The old rule (foam gray >= 0.55 and foam - crema >= 0.15) rejected 4 of 31
// real photos: light crema (Pro_Tulip_2, Michelson ~0.10) and a dark photo
// (Heart_Practice_20, foam gray ~0.45). Measured on those 31 photos (Nov 22):
// Michelson 0.093-0.280, chroma ratio 0.16-0.83, edge width 5.1-13.6 px
// (blurry photos are the widest). Noisy lighting gradient on plain crema
// (self-test): Michelson 0.11, chroma ratio 0.95, edge width 13.7 px, so the
// edge width alone cannot tell it from a blurry photo; the color can.
constexpr float kMinMichelson = 0.06f;
constexpr float kMaxChromaRatio = 0.9f;
constexpr float kMaxEdgeWidth = 16.f;
constexpr int kEdgeKeepOut = 3;  // px: boundary this close to the inner cup edge is the cup cut

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

// Mean steepest gradient around the boundary of the bright class, turned into
// a width: how many px the brightness needs to climb `step`. Boundary pixels
// next to the inner cup edge are skipped (there the class is cut by the mask,
// not by the image). Returns +inf when there is no usable boundary.
float edgeWidthPx(const cv::Mat& blurred, const cv::Mat& bin, const cv::Mat& inner, float step) {
    const int rows = blurred.rows, cols = blurred.cols;
    cv::Mat grad(rows, cols, CV_32FC1);
    for (int y = 0; y < rows; ++y) {
        float* d = grad.ptr<float>(y);
        const int yu = std::max(y - 1, 0), yd = std::min(y + 1, rows - 1);
        for (int x = 0; x < cols; ++x) {
            const int xl = std::max(x - 1, 0), xr = std::min(x + 1, cols - 1);
            const float gx = (blurred.ptr<float>(y)[xr] - blurred.ptr<float>(y)[xl]) / float(std::max(xr - xl, 1));
            const float gy = (blurred.ptr<float>(yd)[x] - blurred.ptr<float>(yu)[x]) / float(std::max(yd - yu, 1));
            d[x] = std::sqrt(gx * gx + gy * gy);
        }
    }

    const cv::Mat usable = basic::erode(inner, kEdgeKeepOut);
    double sum = 0.0, n = 0.0;
    for (int y = 1; y < rows - 1; ++y)
        for (int x = 1; x < cols - 1; ++x) {
            if (!bin.ptr<uchar>(y)[x] || !usable.ptr<uchar>(y)[x]) continue;
            const bool boundary = !bin.ptr<uchar>(y - 1)[x] || !bin.ptr<uchar>(y + 1)[x] ||
                                  !bin.ptr<uchar>(y)[x - 1] || !bin.ptr<uchar>(y)[x + 1];
            if (!boundary) continue;
            float best = 0.f;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const int sx = x + dx, sy = y + dy;
                    if (sx >= 0 && sy >= 0 && sx < cols && sy < rows) best = std::max(best, grad.ptr<float>(sy)[sx]);
                }
            sum += best;
            n += 1.0;
        }
    const double slope = n > 0 ? sum / n : 0.0;
    return slope > 1e-9 ? static_cast<float>(step / slope) : std::numeric_limits<float>::infinity();
}

// The three foam checks (see the constants above) on the raw Otsu split.
FoamEvidence measureFoam(const NormalizedCup& cup, const cv::Mat& blurred, const cv::Mat& inner,
                         const cv::Mat& bin) {
    FoamEvidence ev;
    ev.foam = meanWhere(blurred, inner, bin, true);
    ev.crema = meanWhere(blurred, inner, bin, false);
    if (ev.foam < 0.f || ev.crema < 0.f || ev.foam <= ev.crema) return ev;  // no split / not brighter

    ev.michelson = (ev.foam - ev.crema) / (ev.foam + ev.crema);
    ev.edgeWidth = edgeWidthPx(blurred, bin, inner, ev.foam - ev.crema);

    // Without Lab the color check cannot run; it then passes (ratio 0) and
    // only the brightness checks decide.
    if (!cup.a.empty() && !cup.b.empty()) {
        cv::Mat chroma(cup.a.rows, cup.a.cols, CV_32FC1);
        for (int y = 0; y < chroma.rows; ++y)
            for (int x = 0; x < chroma.cols; ++x) {
                const float a = cup.a.ptr<float>(y)[x], b = cup.b.ptr<float>(y)[x];
                chroma.ptr<float>(y)[x] = std::sqrt(a * a + b * b);
            }
        const float cf = meanWhere(chroma, inner, bin, true), cc = meanWhere(chroma, inner, bin, false);
        ev.chromaRatio = cc > 1e-6f ? cf / cc : (cf > 1e-6f ? std::numeric_limits<float>::infinity() : 0.f);
    }

    ev.hasFoam = ev.michelson >= kMinMichelson && ev.chromaRatio <= kMaxChromaRatio &&
                 ev.edgeWidth <= kMaxEdgeWidth;
    return ev;
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
PatternMask segmentPattern(const NormalizedCup& cup, FoamEvidence* evidence) {
    if (cup.gray.empty() || cup.cupMask.empty())
        throw std::invalid_argument("segmentPattern: cup.gray and cup.cupMask are required");

    const cv::Mat blurred = basic::gaussianBlur(cup.gray, kBlurSigma);
    const cv::Mat inner = basic::erode(cup.cupMask, kRimMargin);

    const float t = basic::otsuThreshold(blurred, inner);
    cv::Mat bin = basic::binarize(blurred, t, inner);

    const FoamEvidence ev = measureFoam(cup, blurred, inner, bin);
    if (evidence) *evidence = ev;
    if (!ev.hasFoam) {
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
