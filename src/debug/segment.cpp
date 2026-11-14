// Owner: Ranju Kim
// --debug segment <canon.png> : pattern mask of a canonical (512) image.
#include <cstdio>

#include "debug/debug.h"
#include "io/io.h"
#include "normalize/manual.h"
#include "structure/segment.h"

namespace la::debug {

int runSegment(const Args& args) {
    if (args.empty()) {
        std::printf("usage: --debug segment <canon.png>   (make it with --canon)\n");
        return 1;
    }
    const NormalizedCup cup = normalize::fromCanonicalImage(io::loadColor(args[0]));
    const PatternMask pm = structure::segmentPattern(cup);
    io::save("debug_segment_mask.png", pm.mask);
    std::printf("area %.0f px (%.1f%% of cup), centroid (%.1f, %.1f)\n", pm.area,
                100.f * pm.area / (3.14159265f * kCanonicalRadius * kCanonicalRadius),
                pm.centroid.x, pm.centroid.y);
    std::printf("saved debug_segment_mask.png\n");
    return 0;
}

}  // namespace la::debug
