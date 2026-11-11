// LatteArt - automated latte art scoring (Team 22)
//
// Usage:
//   LatteArt.exe <reference.jpg> <user.jpg> <pattern:layers> [result.png]
//   e.g. LatteArt.exe data/reference/tulip3.jpg data/user/me01.jpg tulip:3 out.png
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

#include "io/io.h"
#include "normalize/normalize.h"
#include "quality/quality.h"
#include "scoring/scoring.h"
#include "structure/structure.h"
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
    std::printf("Usage: LatteArt.exe <reference.jpg> <user.jpg> <pattern:layers> [result.png]\n");
    std::printf("   ex: LatteArt.exe data/reference/tulip3.jpg data/user/me01.jpg tulip:3 out.png\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        usage();
        return 1;
    }
    const std::string refPath = argv[1], userPath = argv[2];
    const std::string outPath = argc >= 5 ? argv[4] : "result.png";

    std::string stage = "parse arguments";
    try {
        const la::PatternSpec spec = parsePattern(argv[3]);

        stage = "load images";
        const cv::Mat refImg = la::io::loadColor(refPath);
        const cv::Mat userImg = la::io::loadColor(userPath);

        stage = "normalize";
        const la::NormalizedCup ref = la::normalize::normalizeCup(refImg);
        const la::NormalizedCup user = la::normalize::normalizeCup(userImg);
        if (!user.ok) std::printf("[warn] %s\n", user.warning.c_str());

        stage = "segment pattern";
        const la::PatternMask refMask = la::structure::segmentPattern(ref);
        const la::PatternMask userMask = la::structure::segmentPattern(user);

        stage = "count layers";
        const la::LayerResult refLayers = la::structure::analyzeLayers(ref, refMask);
        const la::LayerResult userLayers = la::structure::analyzeLayers(user, userMask);

        stage = "quality metrics";
        const la::QualityResult q = la::quality::evaluateQuality(user, userMask, ref, refMask);

        stage = "scoring";
        const la::ScoreResult s = la::scoring::computeScore(userLayers, spec.targetLayers, q.Q);
        la::viz::printReport(spec, refLayers, userLayers, q, s);

        stage = "render result";
        const cv::Mat vis =
            la::viz::renderResult(ref, refMask, refLayers, user, userMask, userLayers, s);
        la::io::save(outPath, vis);
        la::io::show("LatteArt result", vis);
        la::io::waitKey(0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[error] stage '%s': %s\n", stage.c_str(), e.what());
        return 2;
    }
    return 0;
}
