// Owner: Ranju Kim
// Pattern (milk foam) segmentation inside the normalized cup.
#pragma once
#include "core/types.h"

namespace la::structure {

// Why segmentPattern decided that the cup has milk foam (or not).
// The two classes are the bright and dark side of the Otsu split.
struct FoamEvidence {
    float foam = 0.f, crema = 0.f;  // mean gray (0..1) of the bright / dark class
    float michelson = 0.f;          // (foam - crema) / (foam + crema)
    float edgeWidth = 0.f;          // px over which the boundary between them changes
    float chromaRatio = 0.f;        // chroma of the bright class / dark class (foam is whiter: < 1)
    bool hasFoam = false;
};

// Otsu inside the cup + foam check + morphology + removal of specks and rim
// crescents. Pass `evidence` to see the numbers behind the foam check.
PatternMask segmentPattern(const NormalizedCup& cup, FoamEvidence* evidence = nullptr);

}  // namespace la::structure
