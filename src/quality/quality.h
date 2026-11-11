// Owner: Ranju Kim
// Five quality items, each compared against the expert reference photo.
// Every function returns raw (user), ref (reference), score (0..1) and a
// feedback sentence explaining the deduction.
#pragma once
#include "core/types.h"

namespace la::quality {

struct Weights {  // draft, tune against human ratings
    float contrast = 0.30f;
    float shape = 0.25f;
    float texture = 0.20f;
    float position = 0.15f;
    float size = 0.10f;
};

// Similarity of a user value to the reference value: exp(-|u - r| / sigma).
float closeness(float user, float ref, float sigma);

// Michelson contrast, edge width, high-frequency ratio.
ItemScore evalContrast(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask);

// Mirror IoU and Hu-moment distance to the reference mask.
ItemScore evalShape(const PatternMask& userMask, const PatternMask& refMask);

// GLCM homogeneity, LoG blob count, highlight ratio on the foam.
ItemScore evalTexture(const NormalizedCup& user, const PatternMask& userMask,
                      const NormalizedCup& ref, const PatternMask& refMask);

// Centroid offset from cup center (/ radius) and axis tilt.
ItemScore evalPosition(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask);

// Pattern area / cup area compared with the reference.
ItemScore evalSize(const NormalizedCup& user, const PatternMask& userMask,
                   const NormalizedCup& ref, const PatternMask& refMask);

QualityResult evaluateQuality(const NormalizedCup& user, const PatternMask& userMask,
                              const NormalizedCup& ref, const PatternMask& refMask,
                              const Weights& w = Weights());

}  // namespace la::quality
