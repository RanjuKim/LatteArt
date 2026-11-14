// Owner: Jaeku Kang
// --debug layers <canon.png> : brightness profile along the axis and peaks.
#include <cstdio>

#include "debug/debug.h"
#include "io/io.h"
#include "normalize/manual.h"
#include "structure/layers.h"
#include "structure/segment.h"

namespace la::debug {

int runLayers(const Args& args) {
    if (args.empty()) {
        std::printf("usage: --debug layers <canon.png>   (make it with --canon)\n");
        return 1;
    }
    const NormalizedCup cup = normalize::fromCanonicalImage(io::loadColor(args[0]));
    const PatternMask pm = structure::segmentPattern(cup);
    const LayerResult lr = structure::analyzeLayers(cup, pm);

    std::printf("layers %d (fft %d)%s\n", lr.count, lr.fftCount,
                lr.uncertain ? "  [UNCERTAIN]" : "");
    std::printf("spacing CV %.3f, thickness CV %.3f, U %.3f\n", lr.spacingCV, lr.thicknessCV,
                lr.U);
    std::printf("peaks:");
    for (float p : lr.peakPos) std::printf(" %.0f", p);
    std::printf("\nprofile (index value):\n");
    for (size_t i = 0; i < lr.profile.size(); i += 8) std::printf("  %3zu %.3f\n", i, lr.profile[i]);
    return 0;
}

}  // namespace la::debug
