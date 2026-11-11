// Owner: Ranju Kim
// Pattern (milk foam) segmentation inside the normalized cup.
#pragma once
#include "core/types.h"

namespace la::structure {

// Otsu inside the cup + morphology + largest component.
PatternMask segmentPattern(const NormalizedCup& cup);

}  // namespace la::structure
