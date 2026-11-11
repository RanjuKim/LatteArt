#include "core/todo.h"
#include "quality/quality.h"

namespace la::quality {

ItemScore evalContrast(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask) {
    (void)user; (void)userMask; (void)ref; (void)refMask;
    LA_TODO("evalContrast: Michelson (Imax-Imin)/(Imax+Imin), edge width, HF ratio");
}

}  // namespace la::quality
