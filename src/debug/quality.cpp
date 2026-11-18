// Owner: Ranju Kim
// --debug quality [<ref_canon.png> <user_canon.png>]
//   Always runs the self-tests for the finished quality items.
//   With two canonical images it scores each quality item on its own, so
//   finished items print even while others are still TODO.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "basic/filter.h"
#include "debug/debug.h"
#include "io/io.h"
#include "normalize/manual.h"
#include "quality/quality.h"
#include "structure/segment.h"

namespace la::debug {

namespace {

void printItem(const char* name, const std::function<ItemScore()>& eval) {
    try {
        const ItemScore it = eval();
        std::printf("  %-9s you %.3f / ref %.3f -> score %.2f  %s\n", name, it.raw, it.ref,
                    it.score, it.feedback.c_str());
    } catch (const std::exception& e) {
        std::printf("  %-9s %s\n", name, e.what());
    }
}

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("[FAIL] %s\n", what.c_str());
        throw std::runtime_error("quality self-test failed: " + what);
    }
    std::printf("[PASS] %s\n", what.c_str());
}

// A pattern described only by its area and centroid (enough for size/position).
PatternMask blob(float area, float cx, float cy) {
    PatternMask pm;
    pm.area = area;
    pm.centroid = {cx, cy};
    return pm;
}

void testSizeAndPosition() {
    const NormalizedCup cup;  // canonical frame: center (256, 256), radius 240
    const float c = cup.center.x, cupArea = 3.14159265f * cup.radius * cup.radius;

    const PatternMask same = blob(0.5f * cupArea, c, c);
    check(quality::evalSize(cup, same, cup, same).score > 0.999f, "size: identical -> score 1");
    check(quality::evalPosition(cup, same, cup, same).score > 0.999f, "position: identical -> score 1");

    const ItemScore small = quality::evalSize(cup, blob(0.35f * cupArea, c, c), cup, same);
    check(std::abs(small.raw - 0.35f) < 1e-3f && std::abs(small.ref - 0.5f) < 1e-3f &&
              small.score < 0.5f && small.feedback.find("too small") != std::string::npos,
          "size: 35% vs 50% of the cup -> 'too small'");

    // Proposal example: the expert's pattern sits 50 px toward 9 o'clock.
    const PatternMask ref9 = blob(0.5f * cupArea, c - 50.f, c);
    const ItemScore centered = quality::evalPosition(cup, blob(0.5f * cupArea, c, c), cup, ref9);
    const ItemScore toward3 = quality::evalPosition(cup, blob(0.5f * cupArea, c + 50.f, c), cup, ref9);
    check(centered.score < 1.f && toward3.score < centered.score,
          "position: centered loses points, 3 o'clock loses more (proposal example)");
    check(toward3.feedback.find("3 o'clock") != std::string::npos, "position: feedback names the direction");

    const ItemScore none = quality::evalPosition(cup, blob(0.f, c, c), cup, same);
    check(none.score == 0.f, "position: no pattern -> score 0");

    // Even when the reference pattern is tiny (3% of the cup), no pattern = 0.
    const ItemScore noneSize = quality::evalSize(cup, blob(0.f, c, c), cup, blob(0.03f * cupArea, c, c));
    check(noneSize.score == 0.f && noneSize.feedback == "No pattern detected.",
          "size: no pattern -> score 0, even vs a tiny reference");
}

// ---------------------------------------------------------------------------
// Synthetic canonical cups for contrast / shape / texture.

constexpr int N = kCanonicalSize;

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// Deterministic noise in [-amp, amp].
struct Noise {
    unsigned s = 2024;
    float operator()(float amp) {
        s = s * 1103515245u + 12345u;
        return amp * (static_cast<float>((s >> 8) & 0xFFFF) / 32767.5f - 1.f);
    }
};

using GrayFn = std::function<float(int x, int y)>;

// Builds a gray image from fn, optionally blurs it, and wraps it as a cup.
NormalizedCup makeCup(const GrayFn& fn, float blurSigma = 0.f) {
    cv::Mat g(N, N, CV_32FC1);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) g.ptr<float>(y)[x] = fn(x, y);
    if (blurSigma > 0.f) g = basic::gaussianBlur(g, blurSigma);
    cv::Mat bgr(N, N, CV_8UC3);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const uchar v = static_cast<uchar>(std::clamp(g.ptr<float>(y)[x], 0.f, 1.f) * 255.f + 0.5f);
            uchar* p = bgr.ptr<uchar>(y) + 3 * x;
            p[0] = p[1] = p[2] = v;
        }
    return normalize::fromCanonicalImage(bgr);
}

PatternMask maskFrom(const std::function<bool(int x, int y)>& inside) {
    PatternMask pm;
    pm.mask = cv::Mat(N, N, CV_8UC1);
    double sx = 0, sy = 0, n = 0;
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            const bool in = inside(x, y);
            pm.mask.ptr<uchar>(y)[x] = in ? 255 : 0;
            if (in) { sx += x + 0.5; sy += y + 0.5; n += 1; }
        }
    pm.area = static_cast<float>(n);
    pm.centroid = n > 0 ? cv::Point2f(float(sx / n), float(sy / n)) : cv::Point2f(256.f, 256.f);
    return pm;
}

bool inDisk(int x, int y, float cx, float cy, float r) {
    const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
    return dx * dx + dy * dy <= r * r;
}

PatternMask disk(float cx, float cy, float r) {
    return maskFrom([=](int x, int y) { return inDisk(x, y, cx, cy, r); });
}

// Foam disk of radius 100 at the cup center on crema.
GrayFn foamOnCrema(float foam, float crema) {
    return [=](int x, int y) { return inDisk(x, y, 256.f, 256.f, 100.f) ? foam : crema; };
}

void testContrast() {
    const PatternMask m = disk(256.f, 256.f, 100.f);
    const NormalizedCup crisp = makeCup(foamOnCrema(0.9f, 0.3f));

    const ItemScore same = quality::evalContrast(crisp, m, crisp, m);
    check(same.score > 0.99f && has(same.feedback, "match"), "contrast: identical -> score 1");
    check(std::abs(same.raw - 0.5f) < 0.01f, "contrast: Michelson (0.9-0.3)/(0.9+0.3) = 0.5");

    const ItemScore dull = quality::evalContrast(makeCup(foamOnCrema(0.55f, 0.3f)), m, crisp, m);
    check(dull.score < 0.8f && has(dull.feedback, "contrast is lower"), "contrast: dull foam -> 'contrast is lower'");

    const ItemScore blurry = quality::evalContrast(makeCup(foamOnCrema(0.9f, 0.3f), 4.f), m, crisp, m);
    check(blurry.score < 0.9f && has(blurry.feedback, "blurrier"), "contrast: blurred boundary -> 'blurrier'");

    const ItemScore better = quality::evalContrast(makeCup(foamOnCrema(0.95f, 0.2f)), m, crisp, m);
    check(better.score > 0.99f, "contrast: more contrast than the reference -> no deduction");

    const ItemScore none = quality::evalContrast(crisp, PatternMask{}, crisp, m);
    check(none.score == 0.f && none.feedback == "No pattern detected.", "contrast: no pattern -> score 0");

    // A crisp pattern whose foam runs into the cup rim: the cut at the cup
    // edge must not be measured as a blurry boundary.
    const PatternMask topMask = maskFrom([](int x, int y) {
        return inDisk(x, y, 256.f, 110.f, 150.f) && inDisk(x, y, 256.f, 256.f, 234.f);
    });
    const NormalizedCup touching =
        makeCup([](int x, int y) { return inDisk(x, y, 256.f, 110.f, 150.f) ? 0.9f : 0.3f; });
    const ItemScore rim = quality::evalContrast(touching, topMask, crisp, m);
    std::printf("       (foam touching the rim: %s)\n", rim.feedback.c_str());
    check(!has(rim.feedback, "blurrier") && rim.score > 0.85f,
          "contrast: crisp foam touching the rim is not called blurry");

    // "Foam" darker than the crema (wrong segmentation) must not score.
    const ItemScore inverted = quality::evalContrast(makeCup(foamOnCrema(0.2f, 0.6f)), m, crisp, m);
    check(inverted.score == 0.f && has(inverted.feedback, "Cannot separate"),
          "contrast: foam darker than crema -> not measurable, score 0");
}

// Five overlapping disks (radius 30, 40 px apart) along an axis through the
// cup center, tilted `deg` clockwise; `bend` pushes the middle disks sideways
// in a C shape with zero mean (so the centroid stays centered).
PatternMask diskStack(float deg, float bend = 0.f) {
    const float a = deg * 3.14159265f / 180.f;
    std::vector<cv::Point2f> centers;
    for (int i = -2; i <= 2; ++i) {
        const float t = 40.f * i;
        const float side = bend * ((i * i) / 4.f - 0.4f);  // mean over i = -2..2 is 0
        centers.push_back({256.f + t * std::sin(a) + side * std::cos(a),
                           256.f - t * std::cos(a) + side * std::sin(a)});
    }
    return maskFrom([centers](int x, int y) {
        for (const auto& c : centers)
            if (inDisk(x, y, c.x, c.y, 30.f)) return true;
        return false;
    });
}

// Downward triangle (heart tip without the lobes), rotated `deg` clockwise
// about the cup center.
PatternMask triangle(float deg) {
    const float a = deg * 3.14159265f / 180.f, ca = std::cos(a), sa = std::sin(a);
    return maskFrom([=](int x, int y) {
        // rotate the pixel back by -deg, then test the upright triangle
        const float px = x + 0.5f - 256.f, py = y + 0.5f - 256.f;
        const float ux = px * ca + py * sa + 256.f, uy = -px * sa + py * ca + 256.f;
        const float t = (uy - 160.f) / 200.f;
        return t >= 0.f && t <= 1.f && std::abs(ux - 256.f) <= 110.f * (1.f - t);
    });
}

// Shapes WIDER than tall, rotated `deg` clockwise about the cup center: these
// broke a midline measured across a fixed vertical axis (review finding).
PatternMask rotatedShape(float deg, const std::function<bool(float ux, float uy)>& upright) {
    const float a = deg * 3.14159265f / 180.f, ca = std::cos(a), sa = std::sin(a);
    return maskFrom([=](int x, int y) {
        const float px = x + 0.5f - 256.f, py = y + 0.5f - 256.f;
        return upright(px * ca + py * sa, -px * sa + py * ca);  // coordinates relative to center
    });
}

PatternMask wideEllipse(float deg) {
    return rotatedShape(deg, [](float ux, float uy) { return (ux * ux) / (70.f * 70.f) + (uy * uy) / (50.f * 50.f) <= 1.f; });
}

// Heart: two round lobes on top, a triangular tip below, a crema hole inside.
PatternMask heart(float deg) {
    return rotatedShape(deg, [](float ux, float uy) {
        auto in = [](float x, float y, float cx, float cy, float r) {
            return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
        };
        const bool lobes = in(ux, uy, -50.f, -50.f, 60.f) || in(ux, uy, 50.f, -50.f, 60.f);
        const float t = (uy + 50.f) / 160.f;  // 0 at the lobe centers, 1 at the tip
        const bool tip = t >= 0.f && t <= 1.f && std::abs(ux) <= 108.f * (1.f - t);
        const bool hole = in(ux, uy, 0.f, -10.f, 18.f);
        return (lobes || tip) && !hole;
    });
}

void testAxisTools() {
    const PatternMask round = disk(256.f, 256.f, 100.f);
    check(quality::mirrorIoU(round.mask, quality::AxisLine{round.centroid, 0.f}) > 0.98f,
          "axis: disk is symmetric about the vertical");

    const PatternMask tilted = triangle(12.f);
    const quality::AxisLine found = quality::findSymmetryAxis(tilted.mask, 0.f, 20.f);
    std::printf("       (triangle rotated 12 deg: axis found at %.1f deg, mirror IoU %.3f)\n",
                found.angleDeg, quality::mirrorIoU(tilted.mask, found));
    check(std::abs(found.angleDeg - 12.f) < 1.5f && quality::mirrorIoU(tilted.mask, found) > 0.95f,
          "axis: finds the 12-degree symmetry axis of a rotated triangle");

    // Intended use: find the pattern's own axis, then measure along it.
    auto lean = [](const PatternMask& pm, float& leanDeg) {
        const quality::AxisLine ax = quality::findSymmetryAxis(pm.mask, 0.f, 20.f);
        const quality::AxisShape s = quality::measureAxis(pm.mask, ax);
        leanDeg = ax.angleDeg + s.tiltDeg;
        return s;
    };
    float deg = 0.f;
    const quality::AxisShape right = lean(diskStack(15.f), deg);
    std::printf("       (stack tilted 15 deg: lean %.1f deg, bend %.2f px)\n", deg, right.bendPx);
    check(right.valid && std::abs(deg - 15.f) < 1.f && right.bendPx < 1.f,
          "axis: measures a 15-degree lean (top to the right = positive)");

    const quality::AxisShape left = lean(diskStack(-10.f), deg);
    check(left.valid && std::abs(deg + 10.f) < 1.f, "axis: a lean to the left is negative");

    const quality::AxisShape wide = lean(heart(10.f), deg);
    std::printf("       (heart rotated 10 deg: lean %.1f deg, bend %.2f px)\n", deg, wide.bendPx);
    check(wide.valid && std::abs(deg - 10.f) < 1.f && wide.bendPx < 2.f,
          "axis: a wide heart rotated 10 deg -> lean 10, no bend (review finding)");

    const quality::AxisShape curved =
        quality::measureAxis(diskStack(0.f, 40.f).mask, quality::AxisLine{{256.f, 256.f}, 0.f});
    std::printf("       (C-shaped stack: tilt %.1f deg, bend %.2f px)\n", curved.tiltDeg, curved.bendPx);
    check(curved.valid && std::abs(curved.tiltDeg) < 2.f && curved.bendPx > 8.f,
          "axis: a C-shaped (bent) axis shows bend but no tilt");

    check(!quality::measureAxis(disk(256.f, 256.f, 3.f).mask, quality::AxisLine{{256.f, 256.f}, 0.f}).valid,
          "axis: too small a pattern is not measurable");
}

void testShape() {
    const PatternMask ref = disk(256.f, 256.f, 100.f);
    const ItemScore same = quality::evalShape(ref, ref);
    check(same.score > 0.99f && has(same.feedback, "matches"), "shape: identical -> score 1");

    // Same shape, smaller and shifted: aligning size and position must cancel that.
    const ItemScore scaled = quality::evalShape(disk(230.f, 280.f, 70.f), ref);
    check(scaled.score > 0.9f, "shape: smaller, shifted copy -> still high score");

    // Disk with an extra lobe on the right: less symmetric than the reference.
    const PatternMask lobed = maskFrom([](int x, int y) {
        return inDisk(x, y, 256.f, 256.f, 100.f) || inDisk(x, y, 350.f, 256.f, 45.f);
    });
    const ItemScore asym = quality::evalShape(lobed, ref);
    std::printf("       (lobed: symmetry %.3f vs %.3f, score %.3f, %s)\n", asym.raw, asym.ref, asym.score,
                asym.feedback.c_str());
    check(asym.raw < asym.ref && asym.score < 0.9f && has(asym.feedback, "less symmetric"),
          "shape: one-sided lobe -> 'less symmetric'");

    // Downward triangle (a heart tip without the lobes) vs disk: different form.
    const PatternMask tri = maskFrom([](int x, int y) {
        const float t = (y + 0.5f - 160.f) / 200.f;  // 0 at top, 1 at the tip
        return t >= 0.f && t <= 1.f && std::abs(x + 0.5f - 256.f) <= 110.f * (1.f - t);
    });
    const ItemScore other = quality::evalShape(tri, ref);
    check(other.score < scaled.score - 0.1f, "shape: triangle vs disk scores clearly lower than a scaled disk");

    check(quality::evalShape(PatternMask{}, ref).score == 0.f, "shape: no pattern -> score 0");

    // A symmetric pattern that is tilted a little keeps its symmetry score
    // (the tilt is judged by the position item, not as asymmetry).
    const ItemScore tiltedTri = quality::evalShape(triangle(12.f), triangle(0.f));
    check(tiltedTri.raw > 0.95f && !has(tiltedTri.feedback, "less symmetric"),
          "shape: a 12-degree tilted symmetric pattern is still symmetric");

    PatternMask broken = maskFrom([](int, int) { return false; });
    broken.area = 500.f;  // inconsistent: area says 500 px, mask is empty
    const ItemScore bad = quality::evalShape(broken, ref);
    check(bad.score == 0.f && std::isfinite(bad.score), "shape: area > 0 but empty mask -> 0, no NaN");
}

void testTexture() {
    const PatternMask m = disk(256.f, 256.f, 150.f);
    Noise noise;
    // Smooth foam with a little sensor noise.
    auto smoothFn = [&](float amp) {
        return [=, &noise](int x, int y) {
            return inDisk(x, y, 256.f, 256.f, 150.f) ? 0.85f + noise(amp) : 0.3f;
        };
    };
    const NormalizedCup smooth = makeCup(smoothFn(0.01f));

    const ItemScore same = quality::evalTexture(smooth, m, smooth, m);
    check(same.score > 0.99f && has(same.feedback, "matches"), "texture: identical -> score 1");

    // 25 dark bubbles (radius 3, 0.3 darker) on a 5x5 grid inside the foam.
    auto bubbleFn = [&](int x, int y) {
        if (!inDisk(x, y, 256.f, 256.f, 150.f)) return 0.3f;
        for (int i = 0; i < 5; ++i)
            for (int j = 0; j < 5; ++j)
                if (inDisk(x, y, 176.f + 40.f * i, 176.f + 40.f * j, 3.f)) return 0.55f;
        return 0.85f + noise(0.01f);
    };
    const ItemScore bubbly = quality::evalTexture(makeCup(bubbleFn), m, smooth, m);
    std::printf("       (bubbly foam feedback: %s)\n", bubbly.feedback.c_str());
    check(bubbly.score < 0.9f && has(bubbly.feedback, "Visible bubbles") && has(bubbly.feedback, "(25 found"),
          "texture: 25 bubbles -> all 25 found, 'Visible bubbles'");

    const ItemScore rough = quality::evalTexture(makeCup(smoothFn(0.12f)), m, smooth, m);
    check(rough.raw < rough.ref && rough.score < 0.9f, "texture: rough foam -> lower homogeneity and score");

    // Reference with specular highlights (3% bright pixels), user without.
    auto glossyFn = [&](int x, int y) {
        if (!inDisk(x, y, 256.f, 256.f, 150.f)) return 0.3f;
        return ((x * 7 + y * 13) % 33 == 0) ? 1.f : 0.85f + noise(0.01f);
    };
    const ItemScore matte = quality::evalTexture(smooth, m, makeCup(glossyFn), m);
    check(matte.score < 1.f && has(matte.feedback, "less glossy"), "texture: no highlights vs glossy -> 'less glossy'");

    // Much more shine (30% of the foam) must count as MORE glossy, not less.
    auto veryGlossyFn = [&](int x, int y) {
        if (!inDisk(x, y, 256.f, 256.f, 150.f)) return 0.3f;
        return ((x * 7 + y * 13) % 10 < 3) ? 1.f : 0.85f + noise(0.01f);
    };
    const NormalizedCup veryGlossy = makeCup(veryGlossyFn);
    const ItemScore shiny = quality::evalTexture(veryGlossy, m, makeCup(glossyFn), m);
    check(!has(shiny.feedback, "less glossy"), "texture: 30% highlights vs 3% -> not 'less glossy'");
    const ItemScore dull = quality::evalTexture(makeCup(glossyFn), m, veryGlossy, m);
    check(has(dull.feedback, "less glossy"), "texture: 3% highlights vs 30% -> 'less glossy'");

    // Same foam photographed brighter: texture must not change.
    const NormalizedCup brighter = makeCup([&](int x, int y) {
        return inDisk(x, y, 256.f, 256.f, 150.f) ? 0.93f + noise(0.01f) : 0.38f;
    });
    check(quality::evalTexture(brighter, m, smooth, m).score > 0.99f,
          "texture: brighter exposure of the same foam -> same texture");

    const PatternMask tiny = disk(256.f, 256.f, 25.f);
    check(has(quality::evalTexture(smooth, tiny, smooth, m).feedback, "too small"),
          "texture: foam smaller than 2000 px -> 'too small' instead of a noisy score");

    check(quality::evalTexture(smooth, PatternMask{}, smooth, m).score == 0.f, "texture: no pattern -> score 0");
}


void testPositionAxis() {
    const NormalizedCup cup;
    const PatternMask straight = diskStack(0.f);
    check(quality::evalPosition(cup, straight, cup, straight).score > 0.99f,
          "position: identical straight axis -> score 1");

    const ItemScore lean = quality::evalPosition(cup, diskStack(15.f), cup, straight);
    std::printf("       (leaning stack: %s)\n", lean.feedback.c_str());
    check(lean.score < 0.95f && has(lean.feedback, "leans 15.0 degrees to the right"),
          "position: 15-degree lean -> 'leans 15.0 degrees to the right'");

    const ItemScore bent = quality::evalPosition(cup, diskStack(0.f, 40.f), cup, straight);
    std::printf("       (bent stack: %s)\n", bent.feedback.c_str());
    check(bent.score < 0.95f && has(bent.feedback, "axis is bent"), "position: C-shaped axis -> 'axis is bent'");

    // The reference leans too: matching its lean is not penalized more.
    check(quality::evalPosition(cup, diskStack(8.f), cup, diskStack(8.f)).score > 0.99f,
          "position: same lean as the reference -> no deduction");

    // Wide shapes rotated 10 degrees: a straight axis that leans 10 degrees,
    // NOT a bent axis and NOT a lean to the left (review finding).
    for (const auto& [name, user, upright] :
         {std::tuple<const char*, PatternMask, PatternMask>{"heart", heart(10.f), heart(0.f)},
          std::tuple<const char*, PatternMask, PatternMask>{"wide ellipse", wideEllipse(10.f), wideEllipse(0.f)}}) {
        const ItemScore it = quality::evalPosition(cup, user, cup, upright);
        std::printf("       (%s rotated 10 deg: %s)\n", name, it.feedback.c_str());
        const size_t at = it.feedback.find("leans ");
        const float shown = at == std::string::npos ? -1.f : std::stof(it.feedback.substr(at + 6));
        check(std::abs(shown - 10.f) < 1.f && has(it.feedback, "right") && !has(it.feedback, "bent"),
              std::string("position: ") + name + " rotated 10 deg -> leans ~10 deg right, not bent");
        check(quality::evalPosition(cup, upright, cup, upright).score > 0.99f,
              std::string("position: upright ") + name + " vs itself -> score 1");
    }
}

}  // namespace

int runQuality(const Args& args) {
    testSizeAndPosition();
    testAxisTools();
    testPositionAxis();
    testContrast();
    testShape();
    testTexture();
    std::printf("All quality self-tests passed.\n");

    if (args.size() < 2) {
        std::printf("(add <ref_canon.png> <user_canon.png> to score real images)\n");
        return 0;
    }
    const NormalizedCup ref = normalize::fromCanonicalImage(io::loadColor(args[0]));
    const NormalizedCup user = normalize::fromCanonicalImage(io::loadColor(args[1]));
    const PatternMask refMask = structure::segmentPattern(ref);
    const PatternMask userMask = structure::segmentPattern(user);

    std::printf("quality items:\n");
    printItem("size", [&] { return quality::evalSize(user, userMask, ref, refMask); });
    printItem("position", [&] { return quality::evalPosition(user, userMask, ref, refMask); });
    printItem("contrast", [&] { return quality::evalContrast(user, userMask, ref, refMask); });
    printItem("shape", [&] { return quality::evalShape(userMask, refMask); });
    printItem("texture", [&] { return quality::evalTexture(user, userMask, ref, refMask); });
    return 0;
}

}  // namespace la::debug
