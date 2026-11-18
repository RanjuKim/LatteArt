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

// One-sided version: no deduction when the user is at least as good as the
// reference (e.g. sharper edges than the expert), otherwise closeness().
float notWorse(float user, float ref, float sigma, bool higherIsBetter);

// ---------------------------------------------------------------------------
// Axis tools. Everything works on ONE axis at a time, so a pattern with several
// axes is handled by calling them once per axis (with that axis' part of the
// mask). Until the per-axis data is defined in types.h, the quality items use
// a single axis through the whole pattern.

// A straight line in the image: passes through `origin`; direction angleDeg
// is measured clockwise from straight up (0 = 12 o'clock, 90 = 3 o'clock).
struct AxisLine {
    cv::Point2f origin;
    float angleDeg = 0.f;
};

// IoU of the mask with its mirror image about the axis line (1 = perfectly
// symmetric about that line). Returns 0 for an empty mask.
float mirrorIoU(const cv::Mat& mask, const AxisLine& axis);

// The line through the mask centroid, within +-searchDeg of guessDeg, about
// which the mask is most symmetric. The search is limited on purpose: over
// all directions a disk with a bump on one side is symmetric about the
// horizontal, which is not the pattern's axis.
AxisLine findSymmetryAxis(const cv::Mat& mask, float guessDeg = 0.f, float searchDeg = 20.f);

// Midline of the pattern along an axis: the mask is cut into 1-px slices
// across the axis, the middle of each slice is fitted with a straight line.
// Pass the pattern's own axis (findSymmetryAxis): slices cut across any other
// direction give wrong middles for wide patterns such as a tilted heart.
// The pattern's total lean is then axis.angleDeg + tiltDeg.
struct AxisShape {
    bool valid = false;  // false if fewer than 10 usable slices
    float tiltDeg = 0.f; // midline angle relative to the axis, clockwise positive
                         // (positive = the top leans to the right)
    float bendPx = 0.f;  // RMS distance of the slice middles from that line
    int slices = 0;
};
AxisShape measureAxis(const cv::Mat& mask, const AxisLine& axis);

// Michelson contrast, edge width, high-frequency ratio.
ItemScore evalContrast(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask);

// Mirror IoU and Hu-moment distance to the reference mask.
ItemScore evalShape(const PatternMask& userMask, const PatternMask& refMask);

// GLCM homogeneity, LoG blob count, highlight ratio on the foam.
ItemScore evalTexture(const NormalizedCup& user, const PatternMask& userMask,
                      const NormalizedCup& ref, const PatternMask& refMask);

// Centroid offset vs the reference (/ radius), plus axis tilt and straightness
// (proposal: "centered in the cup with a straight axis").
ItemScore evalPosition(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask);

// Pattern area / cup area compared with the reference.
ItemScore evalSize(const NormalizedCup& user, const PatternMask& userMask,
                   const NormalizedCup& ref, const PatternMask& refMask);

QualityResult evaluateQuality(const NormalizedCup& user, const PatternMask& userMask,
                              const NormalizedCup& ref, const PatternMask& refMask,
                              const Weights& w = Weights());

}  // namespace la::quality
