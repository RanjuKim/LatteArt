// Owner: Jaeku Kang
// --debug normalize <photo.jpg> : edge map and the normalized cup.
#include <cstdio>

#include "basic/color.h"
#include "basic/edge.h"
#include "debug/debug.h"
#include "io/io.h"
#include "normalize/normalize.h"

namespace la::debug {

int runNormalize(const Args& args) {
    if (args.empty()) {
        std::printf("usage: --debug normalize <photo.jpg>\n");
        return 1;
    }
    const cv::Mat bgr = io::loadColor(args[0]);
    const cv::Mat gray = basic::bgrToGray(bgr);

    const cv::Mat edges = basic::canny(gray, 0.05f, 0.15f);
    io::save("debug_normalize_edges.png", edges);

    const NormalizedCup cup = normalize::normalizeCup(bgr);
    io::save("debug_normalize_cup.png", cup.bgr);
    std::printf("tilt ratio %.3f, axis %.1f deg, ok=%d %s\n", cup.tiltRatio, cup.axisAngleDeg,
                cup.ok, cup.warning.c_str());
    std::printf("saved debug_normalize_*.png\n");
    return 0;
}

}  // namespace la::debug
