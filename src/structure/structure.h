// Owner: Jaeku Kang
// Pattern segmentation and layer analysis (core of the project).
#pragma once
#include "core/types.h"

namespace la::structure {

struct LayerParams {
    int columnHalfWidth = 6;      // average 2*6+1 columns around the axis
    float minProminence = 0.08f;  // in gray units (0..1), tune on data
    float minArcRatio = 0.3f;     // arc must extend to both sides of axis
};

// Otsu inside the cup + morphology + largest component.
PatternMask segmentPattern(const NormalizedCup& cup);

// Multi-column brightness profile along the vertical axis.
std::vector<float> axisProfile(const NormalizedCup& cup, const PatternMask& pm, int halfWidth);

// Peaks with prominence >= minProminence. Returns indices.
std::vector<int> findPeaks(const std::vector<float>& profile, float minProminence);

// Coefficient of variation (std / mean). Returns 0 for < 2 values.
float coefficientOfVariation(const std::vector<float>& v);

// Count, spacing, thickness, FFT cross-check, uniformity U.
LayerResult analyzeLayers(const NormalizedCup& cup, const PatternMask& pm,
                          const LayerParams& params = LayerParams());

}  // namespace la::structure
