// Owner: Jaeku Kang
// Structure-first banded scoring:
//   S = L(dn) + [H(dn) - L(dn)] * (0.3 U + 0.7 Q)
//   dn = 0 -> 60..100,  dn = 1 -> 30..59,  dn >= 2 -> 0..29
#pragma once
#include "core/types.h"
#include "structure/layers.h"

namespace la::scoring {

struct Band {
    float low, high;
};

Band bandFor(int deltaN);

ScoreResult computeScore(const LayerResult &layers, int targetLayers, float Q);

// Mean of per-line banded scores. Q is measured once on the original-size cups.
// deltaN=-1 explicitly means no single image-level count error is defined.
ScoreResult computeScore(const structure::StructureComparison &structure, float Q);

} // namespace la::scoring
