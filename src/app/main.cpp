// Owner: SHARED - change only with team agreement (PR approved by all)
// LatteArt - automated latte art scoring (Team 22)
//
// Usage:
//   LatteArt.exe <reference.jpg> <user.jpg> <pattern:layers> [result.png]
//   LatteArt.exe --canon <photo.jpg> <cx> <cy> <r> <out.png>
//   LatteArt.exe --batch <labels.csv> <photo_dir> <out.csv>
//   LatteArt.exe --debug <stage> [args...]   (see src/debug/debug.h)
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/batch.h"
#include "app/pipeline.h"
#include "io/io.h"
#include "normalize/manual.h"
#include "debug/debug.h"
#include "viz/report.h"

namespace {

la::PatternSpec parsePattern(const std::string& s) {
    const auto colon = s.find(':');
    if (colon == std::string::npos) throw std::invalid_argument("pattern must look like tulip:3");
    la::PatternSpec spec;
    spec.name = s.substr(0, colon);
    spec.targetLayers = std::stoi(s.substr(colon + 1));
    if (spec.targetLayers <= 0) throw std::invalid_argument("layer count must be positive");
    return spec;
}

void usage() {
    std::printf(
        "Usage:\n"
        "  LatteArt.exe <reference.jpg> <user.jpg> <pattern:layers> [result.png]\n"
        "  LatteArt.exe --canon <photo.jpg> <cx> <cy> <r> <out.png>\n"
        "  LatteArt.exe --batch <labels.csv> <photo_dir> <out.csv>\n"
        "  LatteArt.exe --debug <preprocess|normalize|segment|layers|quality|viz> [args...]\n");
}

int runCanon(int argc, char** argv) {
    if (argc < 7) {
        usage();
        return 1;
    }
    const cv::Mat bgr = la::io::loadColor(argv[2]);
    const cv::Mat out = la::normalize::canonicalCrop(bgr, std::stof(argv[3]), std::stof(argv[4]),
                                                     std::stof(argv[5]));
    la::io::save(argv[6], out);
    std::printf("saved %s (%dx%d)\n", argv[6], out.cols, out.rows);
    return 0;
}

int runDebug(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 1;
    }
    const std::string stage = argv[2];
    const la::debug::Args args(argv + 3, argv + argc);
    if (stage == "preprocess") return la::debug::runPreprocess(args);
    if (stage == "normalize") return la::debug::runNormalize(args);
    if (stage == "segment") return la::debug::runSegment(args);
    if (stage == "layers") return la::debug::runLayers(args);
    if (stage == "quality") return la::debug::runQuality(args);
    if (stage == "viz") return la::debug::runViz(args);
    usage();
    return 1;
}

int runScore(int argc, char** argv, std::string& stage) {
    stage = "parse arguments";
    const la::PatternSpec spec = parsePattern(argv[3]);
    const std::string outPath = argc >= 5 ? argv[4] : "result.png";

    stage = "load images";
    const cv::Mat refImg = la::io::loadColor(argv[1]);
    const cv::Mat userImg = la::io::loadColor(argv[2]);

    const la::app::PipelineResult r = la::app::runPipeline(refImg, userImg, spec, &stage);
    if (!r.user.ok) std::printf("[warn] %s\n", r.user.warning.c_str());
    la::viz::printReport(spec, r.refLayers, r.userLayers, r.quality, r.score);

    stage = "render result";
    const cv::Mat vis = la::viz::renderResult(r.ref, r.refMask, r.refLayers, r.user, r.userMask,
                                              r.userLayers, r.score);
    la::io::save(outPath, vis);
    la::io::show("LatteArt result", vis);
    la::io::waitKey(0);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }
    const std::string mode = argv[1];
    std::string stage = mode;
    try {
        if (mode == "--canon") return runCanon(argc, argv);
        if (mode == "--batch") return la::app::runBatch(argc - 2, argv + 2);
        if (mode == "--debug") return runDebug(argc, argv);
        if (argc < 4) {
            usage();
            return 1;
        }
        return runScore(argc, argv, stage);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[error] stage '%s': %s\n", stage.c_str(), e.what());
        return 2;
    }
}
