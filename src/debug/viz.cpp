// Owner: Cecelia Hein
// --debug viz <canon.png> : tries every drawing function on a canonical image.
// Each step runs on its own, so finished steps are drawn even while later
// steps are still TODO. Result: debug_viz.png
#include <cstdio>
#include <exception>
#include <functional>

#include "core/types.h"
#include "debug/debug.h"
#include "io/io.h"
#include "viz/draw.h"

namespace la::debug {

namespace {

void tryStep(const char* name, const std::function<void()>& step) {
    try {
        step();
        std::printf("  [ok]   %s\n", name);
    } catch (const std::exception& e) {
        std::printf("  [todo] %s  (%s)\n", name, e.what());
    }
}

}  // namespace

int runViz(const Args& args) {
    if (args.empty()) {
        std::printf("usage: --debug viz <canon.png>\n");
        return 1;
    }
    const cv::Mat cup = io::loadColor(args[0]);
    cv::Mat vis = cup.clone();
    const cv::Point c(kCanonicalSize / 2, kCanonicalSize / 2);

    std::printf("drawing steps:\n");
    tryStep("1-a drawRect", [&] { viz::drawRect(vis, {20, 20}, {120, 70}, viz::kGreen, 2); });
    tryStep("1-b drawCross", [&] { viz::drawCross(vis, c, 12, viz::kRed); });
    tryStep("3   drawText", [&] { viz::drawText(vis, {28, 30}, "SCORE 73.2", viz::kWhite); });
    tryStep("4-a drawLine", [&] { viz::drawLine(vis, {c.x, 16}, {c.x, 496}, viz::kYellow); });
    tryStep("4-b drawCircle", [&] { viz::drawCircle(vis, c, 240, viz::kGreen, 2); });

    cv::Mat out = vis;
    tryStep("2   sideBySide", [&] { out = viz::sideBySide(cup, vis); });

    io::save("debug_viz.png", out);
    std::printf("saved debug_viz.png\n");
    return 0;
}

}  // namespace la::debug
