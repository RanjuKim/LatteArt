// Owner: Jaeku Kang
#include "scoring/scoring.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <stdexcept>

namespace la::scoring {

Band bandFor(int deltaN) {
    if (deltaN < 0)
        throw std::invalid_argument("bandFor: negative count error");
    if (deltaN == 0)
        return {60.f, 100.f};
    if (deltaN == 1)
        return {30.f, 59.f};
    return {0.f, 29.f};
}

ScoreResult computeScore(const LayerResult &layers, int targetLayers, float Q) {
    if (layers.count < 0 || targetLayers <= 0 || !std::isfinite(Q) || !std::isfinite(layers.U))
        throw std::invalid_argument("computeScore: invalid input");
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

ScoreResult computeScore(const structure::StructureComparison &comparison, float Q) {
    structure::requireMatchingSize(comparison);
    if (comparison.lines.empty() || !std::isfinite(Q))
        throw std::invalid_argument("computeScore: no lines or invalid quality");
    ScoreResult out;
    out.deltaN = -1;
    out.Q = std::clamp(Q, 0.f, 1.f);
    for (const auto &line : comparison.lines) {
        out.bandLow += line.bandLow;
        out.bandHigh += line.bandHigh;
        out.U += line.U;
        out.finalScore +=
            line.bandLow + (line.bandHigh - line.bandLow) * (0.3f * line.U + 0.7f * out.Q);
    }
    const float n = float(comparison.lines.size());
    out.bandLow /= n;
    out.bandHigh /= n;
    out.U /= n;
    out.finalScore /= n;
    return out;
}

} // namespace la::scoring
