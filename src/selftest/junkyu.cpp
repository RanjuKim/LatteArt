// Owner: Junkyu Kim - personal sandbox, edit freely.
// Run: LatteArt.exe --selftest junkyu canon.png
#include <cstdio>

#include "basic/color.h"
#include "basic/filter.h"
#include "io/io.h"
#include "selftest/selftest.h"
#include "viz/draw.h"

namespace la::selftest {

int junkyu(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::printf("usage: --selftest junkyu <canon.png>\n");
        return 1;
    }
    // Example: blur check + drawing check on a canonical image.
    const cv::Mat bgr = io::loadColor(args[0]);
    const cv::Mat blurred = basic::gaussianBlur(basic::bgrToGray(bgr), 2.f);
    cv::Mat vis = basic::grayToBgr8(blurred);
    viz::drawCircle(vis, {256, 256}, 240, viz::kGreen, 2);
    viz::drawText(vis, {10, 10}, "SCORE 73.2", viz::kYellow);
    io::save("selftest_junkyu.png", vis);
    std::printf("saved selftest_junkyu.png\n");
    return 0;
}

}  // namespace la::selftest
