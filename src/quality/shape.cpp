#include "core/todo.h"
#include "quality/quality.h"

namespace la::quality {

ItemScore evalShape(const PatternMask& userMask, const PatternMask& refMask) {
    (void)userMask; (void)refMask;
    LA_TODO("evalShape: mirror IoU about the vertical axis + Hu moment distance");
}

}  // namespace la::quality
