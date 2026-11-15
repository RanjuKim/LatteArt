// Owner: Ranju Kim
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

// Position: compare WHERE the pattern sits with where the expert placed it.
// The deduction grows with the distance between the two offset vectors, so a
// pattern shifted to the opposite side of the reference loses the most.
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
    it.score = closeness(diff, 0.f, kPositionSigma);
    if (diff < kPositionOk)
        it.feedback = "Position matches the reference.";
    else
        it.feedback = fmt("Pattern is off the reference position by %.0f%% of the cup radius, ",
                          100.0 * diff) +
                      "toward " + std::to_string(clockHour(d)) + " o'clock.";
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
