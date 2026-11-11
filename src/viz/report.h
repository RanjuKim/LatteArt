// Owner: Junkyu Kim
// Console report and the side-by-side result image.
#pragma once
#include "core/types.h"

namespace la::viz {

void printReport(const PatternSpec& spec, const LayerResult& refLayers,
                 const LayerResult& userLayers, const QualityResult& q, const ScoreResult& s);

// Reference | user cups side by side with detected layers, axis and
// problem areas drawn on top. Returns CV_8UC3.
cv::Mat renderResult(const NormalizedCup& ref, const PatternMask& refMask,
                     const LayerResult& refLayers, const NormalizedCup& user,
                     const PatternMask& userMask, const LayerResult& userLayers,
                     const ScoreResult& s);

}  // namespace la::viz
