#include "core/todo.h"
#include "quality/quality.h"

namespace la::quality {

ItemScore evalTexture(const NormalizedCup& user, const PatternMask& userMask,
                      const NormalizedCup& ref, const PatternMask& refMask) {
    (void)user; (void)userMask; (void)ref; (void)refMask;
    LA_TODO("evalTexture: GLCM homogeneity, LoG blobs, highlight ratio");
}

}  // namespace la::quality
