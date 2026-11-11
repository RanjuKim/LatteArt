#include "quality/quality.h"

#include <cmath>

namespace la::quality {

float closeness(float user, float ref, float sigma) {
    if (sigma <= 0.f) return user == ref ? 1.f : 0.f;
    return std::exp(-std::abs(user - ref) / sigma);
}

QualityResult evaluateQuality(const NormalizedCup& user, const PatternMask& userMask,
                              const NormalizedCup& ref, const PatternMask& refMask,
                              const Weights& w) {
    QualityResult q;
    q.contrast = evalContrast(user, userMask, ref, refMask);
    q.shape = evalShape(userMask, refMask);
    q.texture = evalTexture(user, userMask, ref, refMask);
    q.position = evalPosition(user, userMask, ref, refMask);
    q.size = evalSize(user, userMask, ref, refMask);
    q.Q = w.contrast * q.contrast.score + w.shape * q.shape.score +
          w.texture * q.texture.score + w.position * q.position.score + w.size * q.size.score;
    return q;
}

}  // namespace la::quality
