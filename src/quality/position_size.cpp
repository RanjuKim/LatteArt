// Owner: Ranju Kim
#include "core/todo.h"
#include "quality/quality.h"

namespace la::quality {

ItemScore evalPosition(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask) {
    (void)user; (void)userMask; (void)ref; (void)refMask;
    LA_TODO("evalPosition: |centroid - cup center| / radius vs reference");
}

ItemScore evalSize(const NormalizedCup& user, const PatternMask& userMask,
                   const NormalizedCup& ref, const PatternMask& refMask) {
    (void)user; (void)userMask; (void)ref; (void)refMask;
    LA_TODO("evalSize: pattern area / cup area vs reference");
}

}  // namespace la::quality
