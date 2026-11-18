// Owner: Jaeku Kang
// Layer analysis along the pattern axis (core of the project).
#pragma once
#include "core/types.h"

namespace la::structure {

struct LayerParams {
    int columnHalfWidth = 6;     // legacy single-axis diagnostic only
    float minProminence = 0.08f; // in gray units (0..1), tune on data
    float minArcRatio = 0.3f;    // reserved legacy field; multi-line comparison does not use it
    int scanStep = 8;            // columns in the 256px structure comparison frame
    int stripHalfWidth = 1;
    int smoothRadius = 2;
    int minPeakDistance = 6;
    float positionSigma = 0.05f;  // fraction of normalized pattern height, NOT a cutoff
    float thicknessSigma = 0.25f; // relative FWHM error
    float spacingSigma = 0.25f;   // relative spacing error
    float gapCost = 1.5f;         // alignment cost per missing/extra layer, not score points
    float sizeLimit = 0.20f;
    int coarseAngleStep = 15, refineAngleStep = 3, fineAngleStep = 1;
    int rotationCandidates = 3;
};

struct Peak {
    float position = 0, width = 0;
}; // fraction of pattern height
struct PeakMatch {
    int reference = -1, user = -1;
};
struct LineComparison {
    float x = 0;
    std::vector<Peak> reference, user;
    std::vector<PeakMatch> matches;
    int missing = 0, extra = 0;
    float spacingScore = 0, thicknessScore = 0, U = 0;
    float bandLow = 0, bandHigh = 0;
};
struct StructureComparison {
    std::vector<LineComparison> lines;
    float angleDeg = 0, alignmentScore = 0;
    float referenceWidth = 0, referenceHeight = 0, userWidth = 0, userHeight = 0;
    float widthDifference = 0, heightDifference = 0;
    bool sizeOk = false;
    std::string sizeMessage;
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
