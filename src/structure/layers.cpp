#include <cmath>
#include <numeric>

#include "core/todo.h"
#include "structure/structure.h"

namespace la::structure {

std::vector<float> axisProfile(const NormalizedCup& cup, const PatternMask& pm, int halfWidth) {
    (void)cup; (void)pm; (void)halfWidth;
    LA_TODO("axisProfile");
}

std::vector<int> findPeaks(const std::vector<float>& profile, float minProminence) {
    (void)profile; (void)minProminence;
    LA_TODO("findPeaks: local maxima + prominence rule");
}

float coefficientOfVariation(const std::vector<float>& v) {
    if (v.size() < 2) return 0.f;
    const double mean = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    if (mean <= 0.0) return 0.f;
    double var = 0.0;
    for (float x : v) var += (x - mean) * (x - mean);
    return static_cast<float>(std::sqrt(var / v.size()) / mean);
}

LayerResult analyzeLayers(const NormalizedCup& cup, const PatternMask& pm,
                          const LayerParams& params) {
    // Planned steps:
    //  1. profile = axisProfile(...); smooth
    //  2. peaks = findPeaks(...); drop peaks whose arc is too short
    //  3. spacings, FWHM -> spacingCV, thicknessCV
    //  4. dominant frequency of profile -> fftCount; uncertain if != count
    //  5. U = clamp(1 - 0.5 * (spacingCV + thicknessCV), 0, 1)  (draft)
    (void)cup; (void)pm; (void)params;
    LA_TODO("analyzeLayers");
}

}  // namespace la::structure
