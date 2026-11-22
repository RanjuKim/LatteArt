// Owner: Jaeku Kang
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include "app/pipeline.h"
#include "basic/geometry.h"
#include "debug/debug.h"
#include "io/io.h"
#include "normalize/manual.h"
#include "scoring/scoring.h"
#include "structure/segment.h"

namespace la::debug {
namespace {
void check(bool ok, const char *name) {
    if (!ok)
        throw std::runtime_error(std::string("FAIL: ") + name);
    std::printf("PASS: %s\n", name);
}
struct Fixture {
    NormalizedCup cup;
    PatternMask mask;
};
Fixture fixture(float sx = 1, float sy = 1, float dx = 0, float dy = 0, int missing = -1) {
    cv::Mat bgr(512, 512, CV_8UC3), mask(512, 512, CV_8UC1);
    double sumX = 0, sumY = 0, count = 0;
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x) {
            float u = (x + .5f - 256 - dx) / sx + 256, v = (y + .5f - 256 - dy) / sy + 256;
            bool on = false;
            const float centers[3] = {166, 256, 346}, halfWidths[3] = {65, 95, 125};
            for (int k = 0; k < 3; ++k)
                if (k != missing && std::abs(v - centers[k]) < 15 &&
                    std::abs(u - 256) < halfWidths[k])
                    on = true;
            mask.ptr<uchar>(y)[x] = on ? 255 : 0;
            bool inside = std::hypot(x + .5f - 256, y + .5f - 256) < 240;
            int value = on ? 225 : (inside ? 75 : 25);
            for (int c = 0; c < 3; ++c)
                bgr.ptr<uchar>(y)[3 * x + c] = uchar(value);
            if (on) {
                sumX += x + .5;
                sumY += y + .5;
                ++count;
            }
        }
    Fixture f;
    f.cup = normalize::fromCanonicalImage(bgr);
    f.mask.mask = mask;
    f.mask.area = float(count);
    if (count)
        f.mask.centroid = {float(sumX / count), float(sumY / count)};
    return f;
}
Fixture rotated(const Fixture &f, float angle) {
    Fixture out;
    out.cup = normalize::rotateCup(f.cup, angle);
    out.mask.mask = cv::Mat(512, 512, CV_8UC1);
    auto m = basic::rotationAbout({256, 256}, -angle);
    double sumX = 0, sumY = 0, count = 0;
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x) {
            int sx = int(std::lround(m[0] * x + m[1] * y + m[2])),
                sy = int(std::lround(m[3] * x + m[4] * y + m[5]));
            bool on = sx >= 0 && sy >= 0 && sx < 512 && sy < 512 && f.mask.mask.ptr<uchar>(sy)[sx];
            out.mask.mask.ptr<uchar>(y)[x] = on ? 255 : 0;
            if (on) {
                sumX += x + .5;
                sumY += y + .5;
                ++count;
            }
        }
    out.mask.area = float(count);
    out.mask.centroid = {float(sumX / count), float(sumY / count)};
    return out;
}
Fixture transposed(const Fixture &f) {
    Fixture out;
    cv::Mat bgr(512, 512, CV_8UC3), mask(512, 512, CV_8UC1);
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 512; ++x) {
            for (int c = 0; c < 3; ++c)
                bgr.ptr<uchar>(y)[3*x+c] = f.cup.bgr.ptr<uchar>(x)[3*y+c];
            mask.ptr<uchar>(y)[x] = f.mask.mask.ptr<uchar>(x)[y];
        }
    out.cup = normalize::fromCanonicalImage(bgr);
    out.mask = f.mask;
    out.mask.mask = mask;
    out.mask.centroid = {f.mask.centroid.y, f.mask.centroid.x};
    return out;
}
int selfTest() {
    static_assert(std::is_same_v<StructureComparison, structure::StructureComparison>);
    static_assert(std::is_same_v<LineComparison, structure::LineComparison>);
    using structure::matchLayers;
    using structure::Peak;
    structure::LayerParams p;
    auto peaks = structure::findPeaks({0, 0, 1, 1, 1, 0, 0, .5f, 0}, .1f);
    check(peaks.size() == 2 && peaks[0] == 3 && peaks[1] == 7, "flat-topped peaks count once");
    std::vector<Peak> ref = {{.2f, .05f}, {.4f, .05f}, {.6f, .05f}, {.8f, .05f}};
    auto same = matchLayers(ref, ref, p);
    check(same.matches.size() == 4 && same.U > .999f && same.bandLow == 60,
          "identical layer sequence");
    auto missing = matchLayers(ref, {{.2f, .05f}, {.6f, .05f}, {.8f, .05f}}, p);
    check(missing.missing == 1 && missing.extra == 0 && missing.matches.size() == 3 &&
              missing.matches[1].reference == 2,
          "middle missing layer does not shift correspondence");
    check(missing.spacingScore > .999f && missing.bandLow == 45 && missing.bandHigh == 75,
          "skip gap compares same endpoints and preserves count penalty");
    auto extra = matchLayers({{.2f, .05f}, {.6f, .05f}, {.8f, .05f}}, ref, p);
    check(extra.extra == 1 && extra.missing == 0, "extra layer penalty");
    auto replaced = matchLayers({{.2f, .05f}, {.4f, .05f}},
                                {{.2f, .05f}, {.9f, .05f}}, p);
    check(replaced.reference.size() == replaced.user.size() &&
              replaced.missing == 1 && replaced.extra == 1 && replaced.bandHigh == 100,
          "equal counts retain missing and extra layers in shared result");
    StructureComparison shared;
    // Count penalties are proportional rather than absolute-error buckets.
    std::vector<Peak> ten;
    for (int i = 0; i < 10; ++i) ten.push_back({.05f + .1f * i, .02f});
    const auto nine = matchLayers(ten, std::vector<Peak>(ten.begin(), ten.end() - 1), p);
    const auto eight = matchLayers(ten, std::vector<Peak>(ten.begin(), ten.end() - 2), p);
    const auto one = matchLayers(ten, {ten.front()}, p);
    check(std::abs(nine.bandLow - 54) < .001f && std::abs(nine.bandHigh - 90) < .001f &&
              std::abs(eight.bandHigh - 80) < .001f && std::abs(one.bandLow - 6) < .001f &&
              std::abs(one.bandHigh - 10) < .001f &&
              scoring::computeLineScore(nine, 1) > scoring::computeLineScore(eight, 1) &&
              scoring::computeLineScore(eight, 1) > scoring::computeLineScore(one, 1),
          "10-to-9, 10-to-8 and 10-to-1 count differences receive graduated scores");
    const auto none = matchLayers({}, {}, p);
    const auto absentLine = matchLayers(ten, {}, p);
    check(scoring::computeLineScore(none, 1) == 0 &&
              scoring::computeLineScore(absentLine, 1) == 0,
          "raw empty matches and one-sided missing structure have zero count bounds");
    const auto unpaired = matchLayers({{.1f, .02f}}, {{.9f, .02f}}, p);
    check(unpaired.matches.empty() && unpaired.U == 0 && unpaired.bandLow == 60 &&
              unpaired.bandHigh == 100 && scoring::computeLineScore(unpaired, 1) == 80 &&
              scoring::computeLineScore(unpaired, 1) < scoring::computeLineScore(same, 1),
          "unpaired equal counts keep count bounds but lose correspondence credit");
    shared.sizeOk = true;
    shared.lines = {same, missing, extra, replaced};
    shared.lines[0].coordinate = .1f;
    shared.lines[1].coordinate = .3f;
    shared.lines[2].coordinate = .6f;
    shared.lines[3].coordinate = .9f;
    auto horizontalOnly = same;
    horizontalOnly.direction = ScanDirection::Horizontal;
    shared.lines.push_back(horizontalOnly);
    const StructureComparison delivered = shared;
    bool validMatches = true;
    for (const auto &line : delivered.lines) {
        validMatches &= line.reference.size() == line.matches.size() + line.missing;
        validMatches &= line.user.size() == line.matches.size() + line.extra;
        int lastReference = -1, lastUser = -1;
        for (const auto &match : line.matches) {
            validMatches &= match.reference > lastReference && match.user > lastUser &&
                            match.reference < int(line.reference.size()) &&
                            match.user < int(line.user.size());
            lastReference = match.reference;
            lastUser = match.user;
        }
    }
    check(validMatches && delivered.lines[1].reference.size() == 4 &&
              delivered.lines[1].user.size() == 3 && delivered.lines[1].coordinate == .3f &&
              delivered.lines[1].matches[1].reference == 2,
          "shared result preserves per-line counts, coordinates and skipped-layer matches");
    const auto mixedScore = scoring::computeScore(delivered, 1);
    check(std::abs(mixedScore.vertical.finalScore - 87.5f) < .01f &&
              std::abs(mixedScore.horizontal.finalScore - 100) < .01f &&
              std::abs(mixedScore.finalScore - 93.75f) < .01f,
          "unequal direction counts still receive 50/50 weight");
    auto incomplete = shared;
    incomplete.lines.pop_back();
    bool directionRejected = false;
    try { scoring::computeScore(incomplete, 1); }
    catch (const std::invalid_argument &) { directionRejected = true; }
    check(directionRejected, "missing scan direction does not silently change weights");
    auto jitter = matchLayers(ref, {{.21f, .05f}, {.39f, .05f}, {.61f, .05f}, {.79f, .05f}}, p);
    check(jitter.matches.size() == 4 && jitter.missing == 0 && jitter.U < 1,
          "small location jitter matches but spacing is graded");
    auto thick = matchLayers(ref, {{.2f, .1f}, {.4f, .1f}, {.6f, .1f}, {.8f, .1f}}, p);
    check(thick.matches.size() == 4 && thick.thicknessScore < same.thicknessScore,
          "thickness changes lower score");
    auto f = fixture();
    auto s = structure::compareAtAngle(f.cup, f.mask, f.cup, f.mask, 0, p);
    check(s.sizeOk && s.lines.size() > 10 && s.alignmentScore > 99.9f,
          "identical multi-line score");
    auto final = scoring::computeScore(s, 1);
    auto flat = f;
    flat.cup.gray = f.cup.gray.clone();
    for (int y = 0; y < flat.cup.gray.rows; ++y)
        std::fill(flat.cup.gray.ptr<float>(y),
                  flat.cup.gray.ptr<float>(y) + flat.cup.gray.cols, .4f);
    const auto flatComparison = structure::compareAtAngle(
        flat.cup, flat.mask, flat.cup, flat.mask, 0, p);
    bool legacyEmptyBounds = !flatComparison.lines.empty();
    for (const auto& line : flatComparison.lines)
        legacyEmptyBounds = legacyEmptyBounds && line.reference.empty() && line.user.empty() &&
                            line.U == 0 && line.bandLow == 0 && line.bandHigh == 29;
    check(legacyEmptyBounds &&
              std::abs(scoring::computeScore(flatComparison, 1).finalScore - 14.5f) < .001f &&
              scoring::computeScore(flatComparison, 0).finalScore == 0,
          "selected lines with no peaks on either side preserve legacy 0..29 quality scoring");
    check(final.finalScore > 99.9f,
          "multi-line final score without invented global count");
    auto lower = scoring::computeScore(s, 0);
    check(std::abs(lower.finalScore - 80) < .01f, "band floor and 50/50 structure-quality weighting");
    auto shifted = fixture(1, 1, 17, -9);
    auto ss = structure::compareAtAngle(f.cup, f.mask, shifted.cup, shifted.mask, 0, p);
    check(ss.alignmentScore > 99.9f && ss.sizeOk,
          "bounding-box centers remove translation for structure only");
    auto scaled = fixture(.9f, 1.1f);
    auto sc = structure::compareAtAngle(f.cup, f.mask, scaled.cup, scaled.mask, 0, p);
    std::printf("anisotropic score %.3f size %.3f %.3f\n", sc.alignmentScore, sc.widthDifference,
                sc.heightDifference);
    check(sc.sizeOk && sc.alignmentScore > 92,
          "independent scale normalization within size tolerance");
    auto wrongWidth = fixture(.8f, 1);
    auto sw = structure::compareAtAngle(f.cup, f.mask, wrongWidth.cup, wrongWidth.mask, 0, p);
    check(!sw.sizeOk && sw.sizeMessage == "가로폭 크기가 맞지 않습니다.",
          "exactly 20 percent width rejects");
    auto wrongHeight = fixture(1, .75f);
    auto sh = structure::compareAtAngle(f.cup, f.mask, wrongHeight.cup, wrongHeight.mask, 0, p);
    check(!sh.sizeOk && sh.sizeMessage == "세로폭 크기가 맞지 않습니다.",
          "height rejection message");
    auto wrongBoth = fixture(.75f, .75f);
    auto sb = structure::compareAtAngle(f.cup, f.mask, wrongBoth.cup, wrongBoth.mask, 0, p);
    check(!sb.sizeOk && sb.sizeMessage == "크기가 맞지 않습니다.",
          "both dimensions rejection message");
    bool rejected = false;
    try {
        scoring::computeScore(sw, 1);
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    check(rejected, "size failure prevents final score");
    auto absent = fixture(1, 1, 0, 0, 1);
    auto sm = structure::compareAtAngle(f.cup, f.mask, absent.cup, absent.mask, 0, p);
    check(sm.sizeOk && sm.alignmentScore < s.alignmentScore - 10,
          "missing middle stripe reduces image structure score");
    auto tf = transposed(f), ta = transposed(absent);
    auto st = structure::compareAtAngle(tf.cup, tf.mask, ta.cup, ta.mask, 0, p);
    const auto originalDirections = scoring::computeScore(sm, .8f);
    const auto swappedDirections = scoring::computeScore(st, .8f);
    check(std::abs(originalDirections.vertical.finalScore - swappedDirections.horizontal.finalScore) < .001f &&
              std::abs(originalDirections.horizontal.finalScore - swappedDirections.vertical.finalScore) < .001f &&
              std::abs(originalDirections.finalScore - swappedDirections.finalScore) < .001f,
          "transposition swaps scan directions and preserves their equal-weight score");
    check(std::abs(sm.alignmentScore - st.alignmentScore) < .001f,
          "rotation objective uses the same direction-balanced structure score");
    auto turn = rotated(f, 37);
    auto sr = structure::compareStructure(f.cup, f.mask, turn.cup, turn.mask, p);
    std::printf("rotation selected %.1f score %.3f size %.3f %.3f\n", sr.angleDeg,
                sr.alignmentScore, sr.widthDifference, sr.heightDifference);
    // Both scan directions now contribute: rotated thin edges lose more peak
    // agreement to interpolation than the former vertical-only measurement.
    check(sr.sizeOk && sr.alignmentScore > 80 && std::abs(sr.angleDeg - 323) <= 3,
          "coarse-to-fine recovers 37 degree rotation");
    std::string stage;
    auto pipeline = app::scoreNormalized(f.cup, f.cup, p, &stage);
    check(pipeline.score.finalScore > 90 && !pipeline.structure.lines.empty(),
          "canonical pipeline includes existing quality metrics");
    check(std::abs((pipeline.score.vertical.finalScore + pipeline.score.horizontal.finalScore) / 2 -
                       pipeline.score.finalScore) < .001f,
          "direction means reproduce the pipeline final score");
    PatternSpec unspecified;
    stage.clear();
    try {
        app::runPipeline(cv::Mat(), cv::Mat(), unspecified, &stage);
    } catch (const std::exception &) {
        // Deliberately invalid photo: metadata must not reject before normalization.
    }
    check(stage == "normalize reference",
          "unspecified catalog layer count does not gate the pipeline");
    auto translatedPipeline = app::scoreNormalized(f.cup, shifted.cup, p, &stage);
    check(translatedPipeline.structure.alignmentScore > 99 &&
              translatedPipeline.quality.position.score < 0.9f,
          "structure centering does not erase original quality position");
    rejected = false;
    try {
        app::scoreNormalized(f.cup, wrongBoth.cup, p, &stage);
    } catch (const std::runtime_error &) {
        rejected = stage == "pattern size validation";
    }
    check(rejected, "pipeline stops before quality evaluation on size failure");
    io::save("debug_layers_synthetic_reference.png", f.cup.bgr);
    io::save("debug_layers_synthetic_missing.png", absent.cup.bgr);
    io::save("debug_layers_synthetic_rotated.png", turn.cup.bgr);
    io::save("debug_layers_synthetic_wrong_size.png", wrongBoth.cup.bgr);
    return 0;
}
float number(const std::string &s) {
    size_t end = 0;
    float v = std::stof(s, &end);
    if (end != s.size() || !std::isfinite(v))
        throw std::invalid_argument("invalid numeric option");
    return v;
}
void saveCsv(const structure::StructureComparison &s, const std::string &prefix, float Q,
             bool scored) {
    std::ofstream lines(prefix + "_lines.csv"), matches(prefix + "_matches.csv");
    if (!lines || !matches)
        throw std::runtime_error("cannot write structure CSV files");
    lines << "direction,coordinate,reference_count,user_count,missing,extra,spacing,thickness,U,band_low,band_high,final_score\n";
    matches << "direction,coordinate,reference_index,user_index,reference_position,user_position,reference_width,user_width\n";
    std::ofstream summary(prefix + "_summary.csv");
    if (!summary)
        throw std::runtime_error("cannot write structure summary");
    summary << "angle,structure_score,ref_width,ref_height,user_width,user_height,width_difference,"
               "height_difference,size_ok,Q\n"
            << s.angleDeg << ',' << s.alignmentScore << ',' << s.referenceWidth << ','
            << s.referenceHeight << ',' << s.userWidth << ',' << s.userHeight << ','
            << s.widthDifference << ',' << s.heightDifference << ',' << s.sizeOk << ',';
    if (scored)
        summary << Q;
    summary << '\n';
    cv::Mat diagram(544, 528, CV_8UC3);
    for (int y = 0; y < diagram.rows; ++y)
        for (int x = 0; x < diagram.cols * 3; ++x)
            diagram.ptr<uchar>(y)[x] = 24;
    for (const auto &l : s.lines) {
        lines << directionName(l.direction) << ',' << l.coordinate << ',' << l.reference.size() << ',' << l.user.size() << ',' << l.missing << ','
              << l.extra << ',' << l.spacingScore << ',' << l.thicknessScore << ',' << l.U << ','
              << l.bandLow << ',' << l.bandHigh << ',';
        if (scored)
            lines << scoring::computeLineScore(l, Q);
        lines << '\n';
        for (auto m : l.matches) {
            auto r = l.reference[m.reference], u = l.user[m.user];
            matches << directionName(l.direction) << ',' << l.coordinate << ',' << m.reference << ',' << m.user << ',' << r.position << ','
                    << u.position << ',' << r.width << ',' << u.width << '\n';
        }
        auto show = [&](const std::vector<structure::Peak> &peaks, bool user) {
            for (size_t k = 0; k < peaks.size(); ++k) {
                bool matched = false;
                for (auto m : l.matches)
                    if ((user ? m.user : m.reference) == int(k))
                        matched = true;
                if (!matched) {
                    matches << directionName(l.direction) << ',' << l.coordinate << ',' << (user ? -1 : int(k)) << ',' << (user ? int(k) : -1)
                            << ',';
                    if (user)
                        matches << ',' << peaks[k].position << ",," << peaks[k].width << '\n';
                    else
                        matches << peaks[k].position << ",," << peaks[k].width << ",\n";
                }
                const bool vertical = l.direction == ScanDirection::Vertical;
                const int offsetX = user ? 272 : 0, offsetY = vertical ? 0 : 288;
                const int fixed = int(l.coordinate * 224) + 16;
                int first = int(16 + (peaks[k].position - peaks[k].width / 2) * 224);
                int last = int(16 + (peaks[k].position + peaks[k].width / 2) * 224);
                for (int along = std::max(0, first); along <= std::min(255, last); ++along)
                    for (int across = fixed - 1; across <= fixed + 1; ++across) {
                        const int x = offsetX + (vertical ? across : along);
                        const int y = offsetY + (vertical ? along : across);
                        if (x < 0 || y < 0 || x >= diagram.cols || y >= diagram.rows) continue;
                        auto d = diagram.ptr<uchar>(y) + 3 * x;
                        d[0] = matched ? 80 : 40;
                        d[1] = matched ? 220 : 40;
                        d[2] = matched ? 80 : 240;
                    }
            }
        };
        show(l.reference, false);
        show(l.user, true);
    }
    io::save(prefix + "_structure.png", diagram); // left reference/right user; red = unmatched
}
} // namespace
int runLayers(const Args &args) {
    if (args.empty()) {
        std::printf(
            "usage: --debug layers --self-test\n       --debug layers <reference> <user> "
            "[--canonical] [--structure-only] [--prefix=debug_layers]\n       tuning: "
            "--prominence=0.08 --position-sigma=0.05 --gap-cost=1.5 --scan-step=8\n               "
            "--spacing-sigma=0.25 --thickness-sigma=0.25 --size-limit=0.20 --coarse-step=15\n");
        return 1;
    }
    if (args[0] == "--self-test")
        return selfTest();
    if (args.size() == 1) {
        auto cup = normalize::fromCanonicalImage(io::loadColor(args[0]));
        auto mask = structure::segmentPattern(cup);
        auto r = structure::analyzeLayers(cup, mask);
        std::printf("LEGACY center-axis diagnostic only (not used for scoring): count %d FFT "
                    "%d\nUse two images for multi-line comparison.\n",
                    r.count, r.fftCount);
        return 0;
    }
    structure::LayerParams p;
    bool canonical = false, structureOnly = false;
    std::string prefix = "debug_layers";
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--canonical") {
            canonical = true;
            continue;
        }
        if (args[i] == "--structure-only") {
            structureOnly = true;
            continue;
        }
        auto eq = args[i].find('=');
        if (eq == std::string::npos)
            throw std::invalid_argument("expected --option=value");
        auto key = args[i].substr(0, eq), value = args[i].substr(eq + 1);
        if (key == "--prefix") {
            prefix = value;
            continue;
        }
        float v = number(value);
        if (key == "--prominence")
            p.minProminence = v;
        else if (key == "--position-sigma")
            p.positionSigma = v;
        else if (key == "--gap-cost")
            p.gapCost = v;
        else if (key == "--scan-step")
            p.scanStep = int(v);
        else if (key == "--spacing-sigma")
            p.spacingSigma = v;
        else if (key == "--thickness-sigma")
            p.thicknessSigma = v;
        else if (key == "--size-limit")
            p.sizeLimit = v;
        else if (key == "--coarse-step")
            p.coarseAngleStep = int(v);
        else if (key == "--refine-step")
            p.refineAngleStep = int(v);
        else if (key == "--fine-step")
            p.fineAngleStep = int(v);
        else if (key == "--smooth-radius")
            p.smoothRadius = int(v);
        else if (key == "--min-peak-distance")
            p.minPeakDistance = int(v);
        else if (key == "--strip-half-width")
            p.stripHalfWidth = int(v);
        else
            throw std::invalid_argument("unknown layers option: " + key);
    }
    auto load = [canonical](const std::string &path) {
        auto bgr = io::loadColor(path);
        return canonical ? normalize::fromCanonicalImage(bgr) : normalize::normalizeCup(bgr);
    };
    auto ref = load(args[0]), user = load(args[1]);
    auto rm = structure::segmentPattern(ref), um = structure::segmentPattern(user);
    auto s = structure::compareStructure(ref, rm, user, um, p);
    io::save(prefix + "_reference.png", ref.bgr);
    io::save(prefix + "_user.png", user.bgr);
    io::save(prefix + "_reference_mask.png", rm.mask);
    io::save(prefix + "_user_mask.png", um.mask);
    std::printf("rotation %.1f deg, structure %.3f, lines %zu\nwidth difference %.2f%%, height "
                "difference %.2f%%\n",
                s.angleDeg, s.alignmentScore, s.lines.size(), 100 * s.widthDifference,
                100 * s.heightDifference);
    if (!s.sizeOk) {
        saveCsv(s, prefix, 0, false);
        std::printf("%s\n", s.sizeMessage.c_str());
        return 2;
    }
    if (structureOnly) {
        saveCsv(s, prefix, 0, false);
        return 0;
    }
    // scoreNormalized repeats search deliberately so the public pipeline path is exercised.
    std::string stage;
    auto result = app::scoreNormalized(ref, user, p, &stage);
    saveCsv(result.structure, prefix, result.quality.Q, true);
    io::save(prefix + "_aligned_user.png", result.user.bgr);
    std::printf("quality Q %.4f, structure U %.4f, mean band %.2f..%.2f, FINAL %.3f\n",
                result.quality.Q, result.score.U, result.score.bandLow, result.score.bandHigh,
                result.score.finalScore);
    std::printf("quality: contrast %.3f shape %.3f texture %.3f position %.3f size %.3f\n",
                result.quality.contrast.score, result.quality.shape.score,
                result.quality.texture.score, result.quality.position.score,
                result.quality.size.score);
    std::printf("saved %s_lines.csv, %s_matches.csv and PNG diagnostics\n", prefix.c_str(),
                prefix.c_str());
    return 0;
}
} // namespace la::debug
