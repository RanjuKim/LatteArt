// Owner: Junkyu Kim
#include "viz/report.h"

#include <cstdio>

#include "core/todo.h"

namespace la::viz {

    static void printItem(const char* name, const ItemScore& item,
        int weightPct) {
        std::printf(
            "  %-10s %3d%%    %6.1f / 100    %9.3f    %9.3f\n",
            name, weightPct, item.score * 100.f,
            item.raw, item.ref);

        if (!item.feedback.empty()) {
            std::printf("    Feedback: %s\n", item.feedback.c_str());
        }
    }

    void printReport(const PatternSpec& spec,
        const LayerResult& refLayers,
        const LayerResult& userLayers,
        const QualityResult& q,
        const ScoreResult& s) {
        // Display supplied results only.
        // Layer analysis and score calculation belong to other modules.
        const bool needsReview =
            s.needsReview || refLayers.uncertain || userLayers.uncertain;

        std::printf("\n");
        std::printf("============================================================\n");
        std::printf("                    LATTE ART REPORT\n");
        std::printf("============================================================\n");

        std::printf("Target pattern   : %s\n", spec.name.c_str());
        std::printf("Target layers    : %d\n", spec.targetLayers);
        std::printf("Reference layers : %d\n", refLayers.count);
        std::printf("Your layers      : %d\n", userLayers.count);
        std::printf("Layer error      : %d\n", s.deltaN);

        if (needsReview) {
            std::printf("Status           : UNCERTAIN - manual review required\n");

            if (refLayers.uncertain) {
                std::printf(
                    "  Reference: peak count %d, FFT count %d\n",
                    refLayers.count, refLayers.fftCount);
            }

            if (userLayers.uncertain) {
                std::printf(
                    "  Yours    : peak count %d, FFT count %d\n",
                    userLayers.count, userLayers.fftCount);
            }
        }
        else {
            std::printf("Status           : No uncertainty flag reported\n");
        }

        std::printf("\n");
        std::printf("------------------------------------------------------------\n");
        std::printf("SCORE SUMMARY\n");
        std::printf("------------------------------------------------------------\n");
        std::printf("Score band       : %.0f - %.0f\n",
            s.bandLow, s.bandHigh);
        std::printf("Uniformity U     : %.3f / 1.000\n", s.U);
        std::printf("  Spacing CV     : %.3f\n", userLayers.spacingCV);
        std::printf("  Thickness CV   : %.3f\n", userLayers.thicknessCV);
        std::printf("Quality Q        : %.3f / 1.000\n", s.Q);

        std::printf("\n");
        std::printf("------------------------------------------------------------\n");
        std::printf("QUALITY DETAILS\n");
        std::printf("------------------------------------------------------------\n");
        std::printf(
            "  Item       Weight      Score          You          Ref\n");

        printItem("Contrast", q.contrast, 30);
        printItem("Shape", q.shape, 25);
        printItem("Texture", q.texture, 20);
        printItem("Position", q.position, 15);
        printItem("Size", q.size, 10);

        std::printf("\n");
        std::printf("============================================================\n");
        std::printf("FINAL SCORE      : %.1f / 100%s\n",
            s.finalScore, needsReview ? "  [REVIEW REQUIRED]" : "");
        std::printf("============================================================\n");
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
