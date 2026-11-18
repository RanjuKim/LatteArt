// Owner: Ranju Kim
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "quality/quality.h"

namespace la::quality {

namespace {

// Draft tolerances (fraction of the cup radius / cup area), tune with data.
constexpr float kPositionSigma = 0.10f;  // 10% of the radius off -> score e^-1 = 0.37
constexpr float kSizeSigma = 0.10f;      // 10 percentage points of cup area -> 0.37
// Below these differences the feedback says "matches the reference"; chosen so
// that this only happens when the score is about 0.86 or higher.
constexpr float kPositionOk = 0.015f;
constexpr float kSizeOk = 0.015f;
constexpr float kTiltSigmaDeg = 5.f;   // 5 degrees more tilt than the reference -> 0.37
constexpr float kBendSigma = 0.02f;    // midline wandering 2% of the radius more -> 0.37
constexpr float kBendFloor = 0.01f;    // wobble under 1% of the radius counts as straight
constexpr float kAxisOk = 0.85f;       // tilt / straightness sub-score for a positive message
constexpr float kAxisSearchDeg = 20.f; // symmetry axis searched within +-20 deg of upright

constexpr float kPi = 3.14159265f;

std::string fmt(const char* f, double a, double b = 0.0, double c = 0.0) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), f, a, b, c);
    return buf;
}

// Offset of the pattern centroid from the cup center, in units of the radius.
cv::Point2f offset(const NormalizedCup& cup, const PatternMask& pm) {
    return {(pm.centroid.x - cup.center.x) / cup.radius,
            (pm.centroid.y - cup.center.y) / cup.radius};
}

// Clock direction of a vector in image coordinates (12 = up, 3 = right).
int clockHour(cv::Point2f v) {
    const float angle = std::atan2(v.x, -v.y);  // 0 at 12 o'clock, clockwise
    int hour = static_cast<int>(std::lround(angle / (kPi / 6.f)));
    hour = ((hour % 12) + 12) % 12;
    return hour == 0 ? 12 : hour;
}

float coverage(const NormalizedCup& cup, const PatternMask& pm) {
    return pm.area / (kPi * cup.radius * cup.radius);
}

}  // namespace

// Position (proposal: "centered in the cup with a straight axis"):
//   1. placement (60%): distance between the user and reference centroid
//      offsets, so a pattern on the opposite side of the reference loses most
//   2. tilt (20%): angle of the pattern midline from the upright axis
//   3. straightness (20%): how far the midline wanders from a straight line
//      (a bent / S-shaped axis), relative to the cup radius
// Tilt and straightness use measureAxis() on one axis through the whole
// pattern; with several axes, measure each axis' part separately.
// Only being less straight / more tilted than the reference is penalized.
// raw / ref = distance of each centroid from the cup center (/ radius).
ItemScore evalPosition(const NormalizedCup& user, const PatternMask& userMask,
                       const NormalizedCup& ref, const PatternMask& refMask) {
    ItemScore it;
    const cv::Point2f u = offset(user, userMask), r = offset(ref, refMask);
    it.raw = std::hypot(u.x, u.y);
    it.ref = std::hypot(r.x, r.y);

    if (userMask.area <= 0.f) {
        it.score = 0.f;
        it.feedback = "No pattern detected.";
        return it;
    }

    const cv::Point2f d(u.x - r.x, u.y - r.y);
    const float diff = std::hypot(d.x, d.y);
    const float sPlace = closeness(diff, 0.f, kPositionSigma);

    const std::string placeMsg =
        fmt("Pattern is off the reference position by %.0f%% of the cup radius, ", 100.0 * diff) +
        "toward " + std::to_string(clockHour(d)) + " o'clock.";

    // Measure along each pattern's own symmetry axis (same axis as the shape
    // item); its lean = axis angle + the small remaining midline tilt.
    const AxisLine axU = findSymmetryAxis(userMask.mask, 0.f, kAxisSearchDeg);
    const AxisLine axR = findSymmetryAxis(refMask.mask, 0.f, kAxisSearchDeg);
    const AxisShape au = measureAxis(userMask.mask, axU);
    const AxisShape ar = measureAxis(refMask.mask, axR);
    if (!au.valid || !ar.valid) {  // axis not measurable: placement only
        it.score = sPlace;
        it.feedback = diff < kPositionOk ? "Position matches the reference." : placeMsg;
        return it;
    }
    const float leanU = axU.angleDeg + au.tiltDeg, leanR = axR.angleDeg + ar.tiltDeg;

    // Lean is compared by size only: the goal is a straight, upright axis, so
    // being more upright than the reference is never penalized (even if the
    // reference leans the other way).
    const float sTilt = notWorse(std::abs(leanU), std::abs(leanR), kTiltSigmaDeg, false);
    // Wobble below kBendFloor is the uneven foam edge, not a bent axis.
    const float bendU = std::max(au.bendPx / user.radius, kBendFloor);
    const float bendR = std::max(ar.bendPx / ref.radius, kBendFloor);
    const float sBend = notWorse(bendU, bendR, kBendSigma, false);
    it.score = 0.6f * sPlace + 0.2f * sTilt + 0.2f * sBend;

    // Feedback about the weakest of the three.
    const bool placeOk = diff < kPositionOk;
    if (placeOk && sTilt >= kAxisOk && sBend >= kAxisOk)
        it.feedback = "Position matches the reference and the axis is straight.";
    else if (sBend <= sTilt && (placeOk || sBend < sPlace))
        it.feedback = fmt("Pattern axis is bent: its midline wanders %.1f%% of the cup radius "
                          "from a straight line (reference %.1f%%).",
                          100.0 * au.bendPx / user.radius, 100.0 * ar.bendPx / ref.radius);
    else if (sTilt < sBend && (placeOk || sTilt < sPlace))
        it.feedback = fmt("Pattern axis leans %.1f degrees to the ", std::abs(leanU)) +
                      (leanU > 0 ? "right" : "left") +
                      fmt(" at the top (reference %.1f degrees).", std::abs(leanR));
    else
        it.feedback = placeMsg;
    return it;
}

// Size: share of the cup area covered by the pattern, user vs reference.
ItemScore evalSize(const NormalizedCup& user, const PatternMask& userMask,
                   const NormalizedCup& ref, const PatternMask& refMask) {
    ItemScore it;
    it.raw = coverage(user, userMask);
    it.ref = coverage(ref, refMask);
    it.score = userMask.area > 0.f ? closeness(it.raw, it.ref, kSizeSigma) : 0.f;

    if (userMask.area <= 0.f)
        it.feedback = "No pattern detected.";
    else if (std::abs(it.raw - it.ref) < kSizeOk)
        it.feedback = "Size matches the reference.";
    else
        it.feedback = fmt("Pattern covers %.0f%% of the cup vs %.0f%% in the reference",
                          100.0 * it.raw, 100.0 * it.ref) +
                      (it.raw < it.ref ? " (too small)." : " (too large).");
    return it;
}

}  // namespace la::quality
