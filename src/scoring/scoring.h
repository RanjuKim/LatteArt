// Owner: Jaeku Kang
// Structure-first banded scoring:
//   S = L(dn) + [H(dn) - L(dn)] * (alpha U + (1-alpha) Q)
//   dn = 0 -> 60..100,  dn = 1 -> 30..59,  dn >= 2 -> 0..29
#pragma once
#include "core/types.h"

namespace la::scoring {

constexpr float kStructureWeight = 0.3f;

struct Band {
    float low, high;
};

Band bandFor(int deltaN);

// Shared by final scoring, console output and CSV export to keep the formula identical.
float computeLineScore(const LineComparison &line, float Q);
DirectionScore summarizeDirection(const StructureComparison &structure,
                                  ScanDirection direction, float Q);

// Equal mean of vertical/horizontal direction scores. Both directions required.
// Q is measured once on the original-size cups at the selected rotation.
ScoreResult computeScore(const StructureComparison &structure, float Q);

} // namespace la::scoring
