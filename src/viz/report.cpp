#include "viz/report.h"

#include <cstdio>

#include "core/todo.h"

namespace la::viz {

static void printItem(const char* name, const ItemScore& it, int weightPct) {
    std::printf("  %-9s %3d%%  score %.2f  (you %.3f / ref %.3f)  %s\n", name, weightPct,
                it.score, it.raw, it.ref, it.feedback.c_str());
}

void printReport(const PatternSpec& spec, const LayerResult& refLayers,
                 const LayerResult& userLayers, const QualityResult& q, const ScoreResult& s) {
    std::printf("\n===== Latte Art Score =====\n");
    std::printf("Target   : %s, %d layers\n", spec.name.c_str(), spec.targetLayers);
    std::printf("Layers   : reference %d, yours %d  (error %d)%s\n", refLayers.count,
                userLayers.count, s.deltaN, s.needsReview ? "  [UNCERTAIN - review]" : "");
    std::printf("Band     : %.0f - %.0f\n", s.bandLow, s.bandHigh);
    std::printf("Uniform U: %.2f  (spacing CV %.2f, thickness CV %.2f)\n", s.U,
                userLayers.spacingCV, userLayers.thicknessCV);
    std::printf("Quality Q: %.2f\n", s.Q);
    printItem("contrast", q.contrast, 30);
    printItem("shape", q.shape, 25);
    printItem("texture", q.texture, 20);
    printItem("position", q.position, 15);
    printItem("size", q.size, 10);
    std::printf("FINAL    : %.1f / 100\n", s.finalScore);
}

cv::Mat renderResult(const NormalizedCup& ref, const PatternMask& refMask,
                     const LayerResult& refLayers, const NormalizedCup& user,
                     const PatternMask& userMask, const LayerResult& userLayers,
                     const ScoreResult& s) {
    (void)ref; (void)refMask; (void)refLayers; (void)user; (void)userMask;
    (void)userLayers; (void)s;
    // Draw lines/markers by writing pixels directly (OpenCV drawing functions are not allowed).
    LA_TODO("renderResult");
}

}  // namespace la::viz
