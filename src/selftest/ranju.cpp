// Owner: Ranju Kim - personal sandbox, edit freely.
// Run: LatteArt.exe --selftest ranju ref_canon.png user_canon.png
// (make canonical images first with --canon)
#include <cstdio>

#include "io/io.h"
#include "normalize/manual.h"
#include "quality/quality.h"
#include "selftest/selftest.h"
#include "structure/segment.h"

namespace la::selftest {

int ranju(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::printf("usage: --selftest ranju <ref_canon.png> <user_canon.png>\n");
        return 1;
    }
    const NormalizedCup ref = normalize::fromCanonicalImage(io::loadColor(args[0]));
    const NormalizedCup user = normalize::fromCanonicalImage(io::loadColor(args[1]));

    const PatternMask refMask = structure::segmentPattern(ref);
    const PatternMask userMask = structure::segmentPattern(user);
    io::save("selftest_ranju_mask.png", userMask.mask);

    const ItemScore size = quality::evalSize(user, userMask, ref, refMask);
    std::printf("size: you %.3f / ref %.3f -> %.2f  %s\n", size.raw, size.ref, size.score,
                size.feedback.c_str());
    return 0;
}

}  // namespace la::selftest
