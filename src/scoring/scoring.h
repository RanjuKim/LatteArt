// Owner: Jaeku Kang
// Count-ratio scoring: C = min(reference count, user count) / max(counts).
// Line bounds are L=60*C, H=100*C (C=0 for empty input).
// Selected scanlines with no peaks on either side retain legacy bounds 0..29.
// S = L + (H-L) * (alpha U + (1-alpha) Q).
#pragma once
#include "core/types.h"

namespace la::scoring {

constexpr float kStructureWeight = 0.5f;

struct Band {
    float low, high;
};

// Legacy absolute-error bands, retained for source compatibility only.
Band bandFor(int deltaN);

// Shared by final scoring, console output and CSV export to keep the formula identical.
float computeLineScore(const LineComparison &line, float Q);
DirectionScore summarizeDirection(const StructureComparison &structure,
                                  ScanDirection direction, float Q);

// Equal mean of vertical/horizontal direction scores. Both directions required.
// Q is measured once on the original-size cups at the selected rotation.
ScoreResult computeScore(const StructureComparison &structure, float Q);

} // namespace la::scoring
