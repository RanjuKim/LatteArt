// Owner: Ranju Kim
// --debug segment [canon.png]
//   Always runs the self-tests for threshold / morphology / FFT / segmentation.
//   With an image (canonical 512, make it with --canon) it also saves
//   debug_segment_mask.png and debug_segment_overlay.png (pattern tinted red).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "basic/fft.h"
#include "basic/morphology.h"
#include "basic/threshold.h"
#include "debug/debug.h"
#include "io/io.h"
#include "normalize/manual.h"
#include "structure/segment.h"

namespace la::debug {

namespace {

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("[FAIL] %s\n", what.c_str());
        throw std::runtime_error("segment self-test failed: " + what);
    }
    std::printf("[PASS] %s\n", what.c_str());
}

int countOn(const cv::Mat& bin) {
    int n = 0;
    for (int y = 0; y < bin.rows; ++y)
        for (int x = 0; x < bin.cols; ++x) n += bin.ptr<uchar>(y)[x] != 0;
    return n;
}

cv::Mat filled(int rows, int cols, int type, double v) {
    cv::Mat m(rows, cols, type);
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < cols; ++x) {
            if (type == CV_32FC1) m.ptr<float>(y)[x] = static_cast<float>(v);
            else m.ptr<uchar>(y)[x] = static_cast<uchar>(v);
        }
    return m;
}

void fillRect(cv::Mat& bin, int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) bin.ptr<uchar>(y)[x] = 255;
}

void testThreshold() {
    // Left half 0.2, right half 0.8 -> threshold between them, right half selected.
    cv::Mat img = filled(10, 20, CV_32FC1, 0.2);
    for (int y = 0; y < 10; ++y)
        for (int x = 10; x < 20; ++x) img.ptr<float>(y)[x] = 0.8f;
    const float t = basic::otsuThreshold(img);
    check(t > 0.2f && t < 0.8f, "otsu splits a two-level image");
    const cv::Mat bin = basic::binarize(img, t);
    check(countOn(bin) == 100 && bin.ptr<uchar>(0)[15] == 255 && bin.ptr<uchar>(0)[5] == 0,
          "binarize selects exactly the bright half");

    // Mask covering only the bright half: one level -> threshold at that level.
    cv::Mat mask = filled(10, 20, CV_8UC1, 0);
    fillRect(mask, 10, 0, 19, 9);
    // Mask covering only the bright half: one level -> nothing to split (1.0).
    check(basic::otsuThreshold(img, mask) == 1.f, "otsu: single brightness level -> 1.0 (no split)");
    check(countOn(basic::binarize(img, basic::otsuThreshold(img, mask), mask)) == 0,
          "otsu: single level selects no pixel");
    check(countOn(basic::binarize(img, 0.5f, mask)) == 100, "binarize is zero outside the mask");

    cv::Mat bad = img.clone();
    bad.ptr<float>(3)[3] = std::nanf("");
    bool threw = false;
    try { basic::otsuThreshold(bad); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "otsu rejects NaN pixels");
}

void testMorphology() {
    cv::Mat square = filled(9, 9, CV_8UC1, 0);
    fillRect(square, 2, 2, 6, 6);  // 5x5
    check(countOn(basic::erode(square, 1)) == 9, "erode 5x5 square by r=1 -> 3x3");

    cv::Mat dot = filled(9, 9, CV_8UC1, 0);
    dot.ptr<uchar>(4)[4] = 255;
    check(countOn(basic::dilate(dot, 1)) == 5, "dilate one pixel by r=1 -> plus of 5 pixels");

    cv::Mat speck = square.clone();
    speck.ptr<uchar>(0)[8] = 255;
    const cv::Mat opened = basic::open(speck, 1);
    check(opened.ptr<uchar>(0)[8] == 0 && opened.ptr<uchar>(4)[4] == 255, "open removes an isolated speck");

    cv::Mat hole = square.clone();
    hole.ptr<uchar>(4)[4] = 0;
    check(basic::close(hole, 1).ptr<uchar>(4)[4] == 255, "close fills a one-pixel hole");

    // Components: 3x3 blob (9), 2x2 blob (4), single speck (1).
    cv::Mat comps = filled(12, 12, CV_8UC1, 0);
    fillRect(comps, 0, 0, 2, 2);
    fillRect(comps, 6, 6, 7, 7);
    comps.ptr<uchar>(11)[0] = 255;
    check(countOn(basic::largestComponent(comps)) == 9, "largestComponent keeps the 3x3 blob");
    check(countOn(basic::removeSmallComponents(comps, 4)) == 13,
          "removeSmallComponents keeps both blobs, drops the speck");
    check(countOn(basic::largestComponent(filled(4, 4, CV_8UC1, 0))) == 0,
          "largestComponent of an empty image is empty");
}

void testFft() {
    const std::vector<double> x = {1.0, -2.0, 0.5, 3.0, 0.0, 1.5, -1.0, 2.0};
    std::vector<basic::Complex> a(x.begin(), x.end());
    basic::fft(a);

    double maxErr = 0.0;
    for (size_t k = 0; k < x.size(); ++k) {  // naive DFT for comparison
        basic::Complex s(0.0, 0.0);
        for (size_t n = 0; n < x.size(); ++n)
            s += x[n] * std::polar(1.0, -2.0 * M_PI * double(k) * double(n) / double(x.size()));
        maxErr = std::max(maxErr, std::abs(s - a[k]));
    }
    check(maxErr < 1e-9, "fft matches a direct DFT");

    basic::fft(a, true);
    double invErr = 0.0;
    for (size_t n = 0; n < x.size(); ++n) invErr = std::max(invErr, std::abs(a[n] - basic::Complex(x[n], 0)));
    check(invErr < 1e-9, "inverse fft restores the signal");

    // 64 samples of a cosine with period 8 -> peak at frequency index 64/8 = 8.
    std::vector<float> wave(64);
    for (int i = 0; i < 64; ++i) wave[i] = static_cast<float>(std::cos(2.0 * M_PI * i / 8.0));
    const std::vector<double> mag = basic::magnitudeSpectrum(wave);
    size_t peak = 1;
    for (size_t k = 1; k < mag.size(); ++k)
        if (mag[k] > mag[peak]) peak = k;
    check(mag.size() == 33 && peak == 8, "magnitudeSpectrum peaks at the cosine frequency");

    // Non-power-of-two length: 200 samples with 4 cycles, padded to 256.
    // The peak bin must be converted with cycles = k * 200 / 256 (see fft.h).
    std::vector<float> prof(200);
    for (int i = 0; i < 200; ++i) prof[i] = static_cast<float>(std::cos(2.0 * M_PI * 4.0 * i / 200.0));
    size_t npad = 0;
    const std::vector<double> m200 = basic::magnitudeSpectrum(prof, &npad);
    size_t k200 = 1;
    for (size_t k = 1; k < m200.size(); ++k)
        if (m200[k] > m200[k200]) k200 = k;
    const double cycles = double(k200) * prof.size() / double(npad);
    check(npad == 256 && std::lround(cycles) == 4,
          "padded spectrum: peak bin converts back to 4 cycles over 200 samples");

    const cv::Mat m2 = basic::fft2Magnitude(filled(3, 5, CV_32FC1, 1.0));
    check(m2.rows == 4 && m2.cols == 8 && std::abs(m2.ptr<float>(0)[0] - 15.f) < 1e-4f,
          "fft2Magnitude pads to 4x8, DC = sum of pixels");
}

void testSegmentation() {
    // Canonical cup: brown crema, two separate white blobs (like tulip
    // layers) and a 2x2 white speck that must be ignored.
    const int N = kCanonicalSize;
    cv::Mat bgr(N, N, CV_8UC3);
    auto paint = [&](int cx, int cy, int r, uchar b, uchar g, uchar rr) {
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                const int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy > r * r) continue;
                uchar* p = bgr.ptr<uchar>(y) + 3 * x;
                p[0] = b; p[1] = g; p[2] = rr;
            }
    };
    paint(N / 2, N / 2, 400, 200, 200, 200);  // light-gray table everywhere
    paint(N / 2, N / 2, 240, 40, 80, 130);    // crema inside the rim
    paint(256, 200, 50, 235, 240, 245);       // upper blob
    paint(256, 330, 60, 235, 240, 245);       // lower blob (gap of 20 px)
    for (int y = 120; y < 122; ++y)
        for (int x = 380; x < 382; ++x) {
            uchar* p = bgr.ptr<uchar>(y) + 3 * x;
            p[0] = p[1] = p[2] = 245;
        }

    const PatternMask pm = structure::segmentPattern(normalize::fromCanonicalImage(bgr));
    const float expected = static_cast<float>(M_PI) * (50 * 50 + 60 * 60);
    check(std::abs(pm.area - expected) / expected < 0.05f, "segment area matches both blobs (within 5%)");
    check(pm.mask.ptr<uchar>(200)[256] == 255 && pm.mask.ptr<uchar>(330)[256] == 255,
          "both separate blobs are kept");
    check(pm.mask.ptr<uchar>(121)[381] == 0, "tiny speck is removed");
    check(pm.mask.ptr<uchar>(256)[20] == 0, "nothing outside the cup");
    check(std::abs(pm.centroid.x - 256.5f) < 2.f && pm.centroid.y > 256.f && pm.centroid.y < 290.f,
          "centroid lies between the blobs, closer to the bigger one");

    // Plain crema, no foam: uniform, then with a light-to-dark gradient and
    // noise (Otsu still splits it, but the bright class is not foam).
    paint(N / 2, N / 2, 400, 200, 200, 200);
    paint(N / 2, N / 2, 240, 40, 80, 130);
    check(structure::segmentPattern(normalize::fromCanonicalImage(bgr)).area == 0.f,
          "uniform crema without foam -> empty mask");
    unsigned seed = 12345;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const int dx = x - N / 2, dy = y - N / 2;
            if (dx * dx + dy * dy > 240 * 240) continue;
            seed = seed * 1103515245u + 12345u;
            const int noise = static_cast<int>((seed >> 16) % 31) - 15;
            const int light = 60 - static_cast<int>(std::sqrt(double(dx * dx + dy * dy)) / 4);  // 60 -> 0
            uchar* p = bgr.ptr<uchar>(y) + 3 * x;
            p[0] = static_cast<uchar>(std::clamp(40 + light + noise, 0, 255));
            p[1] = static_cast<uchar>(std::clamp(80 + light + noise, 0, 255));
            p[2] = static_cast<uchar>(std::clamp(130 + light + noise, 0, 255));
        }
    check(structure::segmentPattern(normalize::fromCanonicalImage(bgr)).area == 0.f,
          "crema with lighting gradient and noise, no foam -> empty mask");

    // Cup circle 20 px off: a crescent of the white ceramic rim falls inside
    // the mask. It must be dropped, while a pattern touching the edge stays.
    paint(N / 2, N / 2, 400, 150, 150, 150);  // table
    paint(N / 2 + 20, N / 2, 265, 235, 238, 240);  // white cup rim
    paint(N / 2 + 20, N / 2, 245, 40, 80, 130);    // crema (shifted right)
    paint(256, 110, 60, 235, 240, 245);            // pattern near the top edge
    const PatternMask rim = structure::segmentPattern(normalize::fromCanonicalImage(bgr));
    check(rim.mask.ptr<uchar>(256)[24] == 0, "white rim crescent is not taken as foam");
    check(rim.mask.ptr<uchar>(110)[256] == 255 && rim.mask.ptr<uchar>(60)[256] == 255,
          "pattern near the edge is kept");
}

}  // namespace

int runSegment(const Args& args) {
    testThreshold();
    testMorphology();
    testFft();
    testSegmentation();
    std::printf("All segment self-tests passed.\n");

    if (args.empty()) {
        std::printf("(add <canon.png> to segment a real image; make it with --canon)\n");
        return 0;
    }
    const NormalizedCup cup = normalize::fromCanonicalImage(io::loadColor(args[0]));
    const PatternMask pm = structure::segmentPattern(cup);
    io::save("debug_segment_mask.png", pm.mask);

    cv::Mat overlay = cup.bgr.clone();
    for (int y = 0; y < overlay.rows; ++y) {
        uchar* p = overlay.ptr<uchar>(y);
        const uchar* m = pm.mask.ptr<uchar>(y);
        for (int x = 0; x < overlay.cols; ++x) {
            if (!m[x]) continue;
            p[3 * x + 0] = static_cast<uchar>(p[3 * x + 0] / 2);
            p[3 * x + 1] = static_cast<uchar>(p[3 * x + 1] / 2);
            p[3 * x + 2] = static_cast<uchar>(p[3 * x + 2] / 2 + 127);
        }
    }
    io::save("debug_segment_overlay.png", overlay);

    std::printf("area %.0f px (%.1f%% of cup), centroid (%.1f, %.1f)\n", pm.area,
                100.0 * pm.area / (M_PI * cup.radius * cup.radius), pm.centroid.x, pm.centroid.y);
    std::printf("saved debug_segment_mask.png, debug_segment_overlay.png\n");
    return 0;
}

}  // namespace la::debug
