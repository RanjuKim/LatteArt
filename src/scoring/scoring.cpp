// Owner: Jaeku Kang
#include "scoring/scoring.h"
#include "structure/layers.h"

#include <algorithm>
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

float computeLineScore(const LineComparison &line, float Q) {
    if (!std::isfinite(Q) || !std::isfinite(line.U) || !std::isfinite(line.bandLow) ||
        !std::isfinite(line.bandHigh) || line.bandHigh < line.bandLow)
        throw std::invalid_argument("computeLineScore: invalid input");
    return line.bandLow + (line.bandHigh - line.bandLow) *
           (kStructureWeight * line.U + (1 - kStructureWeight) * std::clamp(Q, 0.f, 1.f));
}

DirectionScore summarizeDirection(const StructureComparison &comparison,
                                  ScanDirection direction, float Q) {
    if (!std::isfinite(Q)) throw std::invalid_argument("invalid quality");
    DirectionScore out;
    for (const auto &line : comparison.lines) {
        if (line.direction != ScanDirection::Vertical && line.direction != ScanDirection::Horizontal)
            throw std::invalid_argument("invalid scan direction");
        if (line.direction != direction) continue;
        ++out.lineCount;
        out.bandLow += line.bandLow;
        out.bandHigh += line.bandHigh;
        out.U += line.U;
        out.alignmentScore += line.bandLow + (line.bandHigh - line.bandLow) * line.U;
        out.finalScore += computeLineScore(line, Q);
    }
    if (!out.lineCount) throw std::invalid_argument("no lines for required scan direction");
    const float n = float(out.lineCount);
    out.bandLow /= n;
    out.bandHigh /= n;
    out.U /= n;
    out.alignmentScore /= n;
    out.finalScore /= n;
    return out;
}

ScoreResult computeScore(const StructureComparison &comparison, float Q) {
    structure::requireMatchingSize(comparison);
    ScoreResult out;
    out.vertical = summarizeDirection(comparison, ScanDirection::Vertical, Q);
    out.horizontal = summarizeDirection(comparison, ScanDirection::Horizontal, Q);
    out.Q = std::clamp(Q, 0.f, 1.f);
    out.bandLow = (out.vertical.bandLow + out.horizontal.bandLow) * .5f;
    out.bandHigh = (out.vertical.bandHigh + out.horizontal.bandHigh) * .5f;
    out.U = (out.vertical.U + out.horizontal.U) * .5f;
    out.finalScore = (out.vertical.finalScore + out.horizontal.finalScore) * .5f;
    return out;
}

} // namespace la::scoring
