// Owner: Junkyu Kim
// Console report and the side-by-side result image.
#pragma once
#include "core/types.h"

namespace la::viz {

void printReport(const PatternSpec& spec, const StructureComparison& structure,
                 const QualityResult& q, const ScoreResult& s);

// Reference | already-rotated user cups, with separate normalized per-line
// diagrams below. No second rotation, or coordinate projection onto raw photos.
// Green = matched, red = missing reference peak, yellow = extra user peak.
cv::Mat renderResult(const NormalizedCup& ref, const NormalizedCup& user,
                     const StructureComparison& structure, const ScoreResult& s);

}  // namespace la::viz
