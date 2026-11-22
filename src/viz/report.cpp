// Owner: Junkyu Kim
#include "viz/report.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include "scoring/scoring.h"
#include "viz/draw.h"

namespace la::viz {
namespace {
void printItem(const char* name, const ItemScore& item, int weightPct) {
    std::printf("  %-10s %3d%%    %6.1f / 100    %9.3f    %9.3f\n",
                name, weightPct, item.score * 100.f, item.raw, item.ref);
    if (!item.feedback.empty())
        std::printf("    Feedback: %s\n", item.feedback.c_str());
}

void validateStructure(const StructureComparison& structure) {
    if (!structure.sizeOk || structure.lines.empty())
        throw std::invalid_argument("report requires a successful structure comparison");
    for (const auto& line : structure.lines) {
        if (!std::isfinite(line.coordinate) || line.coordinate < 0 || line.coordinate > 1)
            throw std::invalid_argument("report: invalid line coordinate");
        for (const auto* peaks : {&line.reference, &line.user})
            for (const auto& peak : *peaks)
                if (!std::isfinite(peak.position) || !std::isfinite(peak.width) ||
                    peak.width < 0)
                    throw std::invalid_argument("report: invalid peak geometry");
        int previousReference = -1, previousUser = -1;
        for (const auto& match : line.matches) {
            if (match.reference <= previousReference || match.user <= previousUser ||
                match.reference >= int(line.reference.size()) || match.user >= int(line.user.size()))
                throw std::invalid_argument("report: invalid peak correspondence");
            previousReference = match.reference;
            previousUser = match.user;
        }
        if (line.missing < 0 || line.extra < 0 ||
            line.reference.size() != line.matches.size() + line.missing ||
            line.user.size() != line.matches.size() + line.extra)
            throw std::invalid_argument("report: inconsistent unmatched counts");
    }
}

// The two diagrams use the same normalized bounding-box coordinates. They are
// intentionally separate from the cups, whose original size/position are preserved.
void drawStructure(cv::Mat& image, const StructureComparison& structure, bool user, int offset,
                   ScanDirection direction) {
    constexpr int left = 32, width = 448, height = 224;
    const int top = direction == ScanDirection::Vertical ? 624 : 912;
    drawText(image, {offset + left, top - 30},
             std::string(directionName(direction)) + " SCAN", kWhite, 2);
    drawRect(image, {offset + left, top}, {offset + left + width, top + height}, Color{70, 70, 70});
    for (const auto& line : structure.lines) {
        if (line.direction != direction) continue;
        const bool vertical = direction == ScanDirection::Vertical;
        auto point = [&](float along) {
            return cv::Point{offset + left + int(std::lround((vertical ? line.coordinate : along) * width)),
                             top + int(std::lround((vertical ? along : line.coordinate) * height))};
        };
        drawLine(image, point(0), point(1), Color{35, 35, 35});
        const auto& peaks = user ? line.user : line.reference;
        for (size_t i = 0; i < peaks.size(); ++i) {
            bool matched = false;
            for (const auto& match : line.matches)
                if ((user ? match.user : match.reference) == int(i)) matched = true;
            const Color color = matched ? kGreen : (user ? kYellow : kRed);
            const auto& peak = peaks[i];
            drawLine(image, point(std::clamp(peak.position - peak.width / 2, 0.f, 1.f)),
                     point(std::clamp(peak.position + peak.width / 2, 0.f, 1.f)), color, 3);
        }
    }
}
} // namespace

void printReport(const PatternSpec& spec, const StructureComparison& structure,
                 const QualityResult& q, const ScoreResult& s) {
    validateStructure(structure);
    std::printf("\n==================== LATTE ART REPORT ====================\n");
    std::printf("Target pattern   : %s\n", spec.name.c_str());
    if (spec.targetLayers > 0)
        std::printf("Catalog layers   : %d (metadata only)\n", spec.targetLayers);
    std::printf("Rotation applied : %.1f deg clockwise\n", structure.angleDeg);
    std::printf("Width/height diff: %.2f%% / %.2f%%\n",
                100 * structure.widthDifference, 100 * structure.heightDifference);
    std::printf("Compared lines   : %zu\n", structure.lines.size());
    std::printf("\n  LINE  DIRECTION  COORD   REF USER MISS EXTRA SPACING THICKNESS     U SCORE\n");
    size_t matchedLines = 0;
    for (size_t i = 0; i < structure.lines.size(); ++i) {
        const auto& line = structure.lines[i];
        if (!line.reference.empty() && line.missing == 0 && line.extra == 0) ++matchedLines;
        std::printf("  %4zu %10s %5.3f  %4zu %4zu %4d %4d    %.3f      %.3f  %.3f %6.2f\n",
                    i, directionName(line.direction), line.coordinate, line.reference.size(), line.user.size(), line.missing, line.extra,
                    line.spacingScore, line.thicknessScore, line.U,
                    scoring::computeLineScore(line, s.Q));
    }
    std::printf("Lines with all peaks matched: %zu / %zu (not a photo layer count)\n",
                matchedLines, structure.lines.size());
    std::printf("\nSCORE SUMMARY\n");
    std::printf("Vertical score   : %.3f (%zu lines)\n", s.vertical.finalScore, s.vertical.lineCount);
    std::printf("Horizontal score : %.3f (%zu lines)\n", s.horizontal.finalScore, s.horizontal.lineCount);
    std::printf("Direction weights: 50%% / 50%%\n");
    std::printf("Mean line bounds : %.2f - %.2f\n", s.bandLow, s.bandHigh);
    std::printf("Structure U      : %.3f / 1.000\nQuality Q        : %.3f / 1.000\n", s.U, s.Q);
    std::printf("\nQUALITY DETAILS\n  Item       Weight      Score          You          Ref\n");
    printItem("Contrast", q.contrast, 30);
    printItem("Shape", q.shape, 25);
    printItem("Texture", q.texture, 20);
    printItem("Position", q.position, 15);
    printItem("Size", q.size, 10);
    std::printf("\nFINAL SCORE      : %.3f / 100%s\n", s.finalScore,
                s.needsReview ? "  [REVIEW REQUIRED]" : "");
    std::printf("==========================================================\n");
}

cv::Mat renderResult(const NormalizedCup& ref, const NormalizedCup& user,
                     const StructureComparison& structure, const ScoreResult& s) {
    validateStructure(structure);
    for (const auto* cup : {&ref, &user})
        if (!cup->ok || cup->bgr.type() != CV_8UC3 ||
            cup->bgr.rows != kCanonicalSize || cup->bgr.cols != kCanonicalSize)
            throw std::invalid_argument("renderResult: normalized 512x512 color cups required");
    constexpr int offset = kCanonicalSize + 16;
    cv::Mat out(1232, 2 * kCanonicalSize + 16, CV_8UC3);
    for (int y = 0; y < out.rows; ++y)
        std::fill(out.ptr<uchar>(y), out.ptr<uchar>(y) + out.cols * 3, uchar(20));
    for (int side = 0; side < 2; ++side) {
        const auto& cup = side ? user : ref;
        for (int y = 0; y < cup.bgr.rows; ++y)
            std::copy(cup.bgr.ptr<uchar>(y), cup.bgr.ptr<uchar>(y) + cup.bgr.cols * 3,
                      out.ptr<uchar>(64 + y) + side * offset * 3);
    }
    drawText(out, {24, 20}, "REFERENCE", kWhite, 3);
    drawText(out, {offset + 24, 20}, "USER - ALIGNED", kWhite, 3);
    for (auto direction : {ScanDirection::Vertical, ScanDirection::Horizontal}) {
        drawStructure(out, structure, false, 0, direction);
        drawStructure(out, structure, true, offset, direction);
    }
    drawText(out, {32, 1154}, "MATCHED", kGreen, 2);
    drawText(out, {240, 1154}, "MISSING", kRed, 2);
    drawText(out, {offset + 32, 1154}, "MATCHED", kGreen, 2);
    drawText(out, {offset + 240, 1154}, "EXTRA", kYellow, 2);
    char caption[160];
    std::snprintf(caption, sizeof(caption), "SCORE %.2f / 100   U %.3f   Q %.3f   ROTATION %.1f DEG",
                  s.finalScore, s.U, s.Q, structure.angleDeg);
    drawText(out, {24, 1180}, caption, kWhite, 2);
    std::snprintf(caption, sizeof(caption), "VERTICAL %.2f  HORIZONTAL %.2f  WEIGHTS 50/50",
                  s.vertical.finalScore, s.horizontal.finalScore);
    drawText(out, {24, 1206}, caption, kWhite, 2);
    return out;
}
} // namespace la::viz
