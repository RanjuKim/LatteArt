// Owner: Jaeku Kang
// Structure-first banded scoring:
//   S = L(dn) + [H(dn) - L(dn)] * (0.3 U + 0.7 Q)
//   dn = 0 -> 60..100,  dn = 1 -> 30..59,  dn >= 2 -> 0..29
#pragma once
#include "core/types.h"

namespace la::scoring {

struct Band {
    float low, high;
};

Band bandFor(int deltaN);

ScoreResult computeScore(const LayerResult& layers, int targetLayers, float Q);

}  // namespace la::scoring
