// Owner: Ranju Kim
// --debug quality [<ref_canon.png> <user_canon.png>]
//   Always runs the self-tests for the finished quality items.
//   With two canonical images it scores each quality item on its own, so
//   finished items print even while others are still TODO.
#include <cmath>
#include <cstdio>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>

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
}

}  // namespace

int runQuality(const Args& args) {
    testSizeAndPosition();
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
