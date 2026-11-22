// Owner: Jaeku Kang
// Layer analysis along vertical and horizontal sampling lines.
#pragma once
#include "core/types.h"

namespace la::structure {

struct LayerParams {
    int columnHalfWidth = 6;     // legacy single-axis diagnostic only
    float minProminence = 0.08f; // in gray units (0..1), tune on data
    float minArcRatio = 0.3f;    // reserved legacy field; multi-line comparison does not use it
    int scanStep = 8;            // fixed x/y step in the 256px structure comparison frame
    int stripHalfWidth = 1;
    int smoothRadius = 2;
    int minPeakDistance = 6;
    float positionSigma = 0.05f;  // fraction along normalized scan direction, NOT a cutoff
    float thicknessSigma = 0.25f; // relative FWHM error
    float spacingSigma = 0.25f;   // relative spacing error
    float gapCost = 1.5f;         // alignment cost per missing/extra layer, not score points
    float sizeLimit = 0.20f;
    int coarseAngleStep = 15, refineAngleStep = 3, fineAngleStep = 1;
    int rotationCandidates = 3;
};

// Source-compatible names for callers using the former module-local types.
// The sole definitions now live in core/types.h and are shared by all modules.
using ::la::Peak;
using ::la::PeakMatch;
using ::la::LineComparison;
using ::la::StructureComparison;

// Single center-line diagnostic only. Never used as a whole-photo count/result.
struct LayerResult {
    int count = 0, fftCount = 0;
    bool uncertain = false;
    std::vector<float> profile, peakPos, spacings, fwhm;
    float spacingCV = 0, thicknessCV = 0, U = 0;
};

LineComparison matchLayers(const std::vector<Peak> &reference, const std::vector<Peak> &user,
                           const LayerParams &params = LayerParams());
StructureComparison compareAtAngle(const NormalizedCup &ref, const PatternMask &refMask,
                                   const NormalizedCup &user, const PatternMask &userMask,
                                   float angleDeg, const LayerParams &params = LayerParams());
StructureComparison compareStructure(const NormalizedCup &ref, const PatternMask &refMask,
                                     const NormalizedCup &user, const PatternMask &userMask,
                                     const LayerParams &params = LayerParams());
void requireMatchingSize(const StructureComparison &result);

// Multi-column brightness profile along the vertical axis.
std::vector<float> axisProfile(const NormalizedCup &cup, const PatternMask &pm, int halfWidth);

// Peaks with prominence >= minProminence. Returns indices.
std::vector<int> findPeaks(const std::vector<float> &profile, float minProminence);

// Coefficient of variation (std / mean). Returns 0 for < 2 values.
float coefficientOfVariation(const std::vector<float> &v);

// Count, spacing, thickness, FFT cross-check, uniformity U.
LayerResult analyzeLayers(const NormalizedCup &cup, const PatternMask &pm,
                          const LayerParams &params = LayerParams());

} // namespace la::structure
