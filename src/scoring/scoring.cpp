#include "scoring/scoring.h"

#include <algorithm>
#include <cstdlib>

namespace la::scoring {

Band bandFor(int deltaN) {
    if (deltaN == 0) return {60.f, 100.f};
    if (deltaN == 1) return {30.f, 59.f};
    return {0.f, 29.f};
}

ScoreResult computeScore(const LayerResult& layers, int targetLayers, float Q) {
    ScoreResult r;
    r.deltaN = std::abs(layers.count - targetLayers);
    const Band band = bandFor(r.deltaN);
    r.bandLow = band.low;
    r.bandHigh = band.high;
    r.U = std::clamp(layers.U, 0.f, 1.f);
    r.Q = std::clamp(Q, 0.f, 1.f);
    r.finalScore = band.low + (band.high - band.low) * (0.3f * r.U + 0.7f * r.Q);
    r.needsReview = layers.uncertain;
    return r;
}

}  // namespace la::scoring
