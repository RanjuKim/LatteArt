// Owner: Ranju Kim
// --debug quality <ref_canon.png> <user_canon.png> : each quality item on its
// own, so finished items print even while others are still TODO.
#include <cstdio>
#include <exception>
#include <functional>

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

}  // namespace

int runQuality(const Args& args) {
    if (args.size() < 2) {
        std::printf("usage: --debug quality <ref_canon.png> <user_canon.png>\n");
        return 1;
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
