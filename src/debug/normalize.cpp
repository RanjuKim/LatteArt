// Owner: Jaeku Kang
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include "basic/color.h"
#include "basic/edge.h"
#include "basic/geometry.h"
#include "debug/debug.h"
#include "io/io.h"
#include "normalize/normalize.h"

namespace la::debug {
namespace {
void check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(std::string("FAIL: ") + name);
    std::printf("PASS: %s\n", name);
}
cv::Mat syntheticCup(bool cup = true) {
    cv::Mat img(512, 512, CV_8UC3);
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x) {
            double dx = x - 257.0, dy = (y - 249.0) / 0.90, r = std::hypot(dx, dy);
            int v = 35;
            if (cup && r < 211)
                v = 230;
            if (cup && r < 190)
                v = 80 + ((x + 3 * y) % 7);
            if (cup && r < 150 && std::abs(x - 257) < 75 &&
                ((y > 180 && y < 207) || (y > 240 && y < 267) || (y > 300 && y < 327)))
                v = 220;
            for (int c = 0; c < 3; ++c)
                img.ptr<uchar>(y)[3 * x + c] = uchar(v);
        }
    return img;
}
int selfTest() {
    cv::Mat ramp(12, 16, CV_32FC1);
    for (int y = 0; y < ramp.rows; ++y)
        for (int x = 0; x < ramp.cols; ++x)
            ramp.ptr<float>(y)[x] = float(x) / 15;
    cv::Mat gx, gy;
    basic::sobel(ramp, gx, gy);
    check(std::abs(gx.ptr<float>(6)[8] - 8.f / 15) < 1e-5f && std::abs(gy.ptr<float>(6)[8]) < 1e-5f,
          "Sobel ramp sign and scale");
    auto identity = basic::warpPerspective(ramp, basic::identity(), {16, 12});
    check(identity.ptr<float>(11)[15] == 1 && identity.ptr<float>(6)[8] == ramp.ptr<float>(6)[8],
          "identity warp includes last row/column");
    auto shifted = basic::warpPerspective(ramp, basic::translation(.5, 0), {16, 12});
    check(std::abs(shifted.ptr<float>(5)[5] - 5.5f / 15) < 1e-5f, "bilinear half-pixel sampling");
    check(shifted.ptr<float>(5)[15] == 0, "outside sampling is zero");
    bool singular = false;
    try {
        basic::inverse({0, 0, 0, 0, 0, 0, 0, 0, 0});
    } catch (const std::invalid_argument &) {
        singular = true;
    }
    check(singular, "singular matrix rejected");
    cv::Mat step(64, 64, CV_32FC1);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            step.ptr<float>(y)[x] = x < 32 ? 0.f : 1.f;
    auto edges = basic::canny(step, .05f, .15f);
    int found = 0;
    for (int y = 5; y < 59; ++y)
        for (int x = 29; x < 35; ++x)
            found += edges.ptr<uchar>(y)[x] != 0;
    check(found >= 50, "Canny detects straight boundary");
    std::vector<cv::Point2f> points;
    double a = .4;
    for (int i = 0; i < 180; ++i) {
        double t = i * 6.283185307179586 / 180, u = 130 * std::cos(t), v = 90 * std::sin(t);
        points.emplace_back(float(250 + std::cos(a) * u - std::sin(a) * v),
                            float(240 + std::sin(a) * u + std::cos(a) * v));
    }
    auto e = normalize::fitEllipse(points);
    check(std::abs(e.center.x - 250) < .1 && std::abs(e.center.y - 240) < .1 &&
              std::abs(e.a - 130) < .1 && std::abs(e.b - 90) < .1,
          "ellipse center and axes");
    bool rejected = false;
    try {
        normalize::fitEllipse(std::vector<cv::Point2f>(10, {1, 1}));
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    check(rejected, "degenerate ellipse rejected");
    auto img = syntheticCup();
    auto detected = normalize::detectCup(img);
    std::printf("detected liquid: %.2f %.2f axes %.2f %.2f\n", detected.liquid.center.x,
                detected.liquid.center.y, detected.liquid.a, detected.liquid.b);
    check(std::abs(detected.liquid.center.x - 257) < 4 &&
              std::abs(detected.liquid.center.y - 249) < 4,
          "cup center detection");
    check(std::abs(detected.liquid.a - 190) < 10 && std::abs(detected.liquid.b - 171) < 10,
          "liquid boundary rather than outer ceramic");
    auto cup = normalize::normalizeCup(img);
    check(cup.ok && cup.bgr.rows == 512 && cup.gray.type() == CV_32FC1 && !cup.L.empty(),
          "normalized cup contract");
    rejected = false;
    try {
        normalize::detectCup(syntheticCup(false));
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    check(rejected, "no-cup image rejected");
    io::save("debug_normalize_synthetic.png", img);
    io::save("debug_normalize_synthetic_canon.png", cup.bgr);
    return 0;
}
float number(const std::string &value) {
    size_t used = 0;
    float v = std::stof(value, &used);
    if (used != value.size() || !std::isfinite(v))
        throw std::invalid_argument("invalid number");
    return v;
}
} // namespace
int runNormalize(const Args &args) {
    if (args.empty()) {
        std::printf(
            "usage: --debug normalize <photo> [--texture-weight=0.25] [--glcm-radius=3] "
            "[--canny-low=0.05] [--canny-high=0.15]\n       --debug normalize --self-test\n");
        return 1;
    }
    if (args[0] == "--self-test")
        return selfTest();
    normalize::NormalizeParams p;
    for (size_t i = 1; i < args.size(); ++i) {
        auto eq = args[i].find('=');
        if (eq == std::string::npos)
            throw std::invalid_argument("expected --option=value");
        auto key = args[i].substr(0, eq);
        float v = number(args[i].substr(eq + 1));
        if (key == "--texture-weight")
            p.textureWeight = v;
        else if (key == "--glcm-radius")
            p.glcmPatchRadius = int(v);
        else if (key == "--glcm-levels")
            p.glcmLevels = int(v);
        else if (key == "--inner-boundary-support")
            p.innerBoundarySupport = v;
        else if (key == "--canny-low")
            p.cannyLow = v;
        else if (key == "--canny-high")
            p.cannyHigh = v;
        else if (key == "--min-axis-ratio")
            p.minAxisRatio = v;
        else
            throw std::invalid_argument("unknown normalize option: " + key);
    }
    auto bgr = io::loadColor(args[0]);
    auto d = normalize::detectCup(bgr, p);
    auto edges = basic::canny(basic::bgrToGray(bgr), p.cannyLow, p.cannyHigh);
    io::save("debug_normalize_edges.png", edges);
    // Own raster overlay, independent of unfinished viz/draw.
    auto overlay = bgr.clone();
    auto draw = [&](const normalize::Ellipse &e, int channel) {
        double a = e.angleDeg * 3.14159265358979323846 / 180;
        for (int i = 0; i < 2000; ++i) {
            double t = i * 6.283185307179586 / 2000, u = e.a * std::cos(t), v = e.b * std::sin(t);
            int x = int(std::lround(e.center.x + std::cos(a) * u - std::sin(a) * v)),
                y = int(std::lround(e.center.y + std::sin(a) * u + std::cos(a) * v));
            if (x >= 0 && y >= 0 && x < overlay.cols && y < overlay.rows)
                for (int c = 0; c < 3; ++c)
                    overlay.ptr<uchar>(y)[3 * x + c] = c == channel ? 255 : 0;
        }
    };
    draw(d.rim, 2);
    draw(d.liquid, 1);
    io::save("debug_normalize_boundaries.png", overlay);
    auto cup = normalize::normalizeCup(bgr, p);
    io::save("debug_normalize_cup.png", cup.bgr);
    io::save("debug_normalize_mask.png", cup.cupMask);
    std::printf("liquid center %.2f %.2f, axes %.2f %.2f, coverage %.3f, fit error %.4f\n",
                d.liquid.center.x, d.liquid.center.y, d.liquid.a, d.liquid.b, d.coverage,
                d.fitError);
    std::printf("saved debug_normalize_*.png (rim red, liquid green)\n");
    return 0;
}
} // namespace la::debug
