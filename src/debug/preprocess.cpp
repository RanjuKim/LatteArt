// Owner: Junkyu Kim
// --debug preprocess <photo.jpg> : gray, Gaussian blur, CLAHE, Lab L channel.
#include <cstdio>

#include "basic/clahe.h"
#include "basic/color.h"
#include "basic/filter.h"
#include "debug/debug.h"
#include "io/io.h"

namespace la::debug {

int runPreprocess(const Args& args) {
    if (args.empty()) {
        std::printf("usage: --debug preprocess <photo.jpg>\n");
        return 1;
    }
    const cv::Mat bgr = io::loadColor(args[0]);
    const cv::Mat gray = basic::bgrToGray(bgr);
    io::save("debug_preprocess_gray.png", basic::grayToBgr8(gray));

    const cv::Mat blurred = basic::gaussianBlur(gray, 2.f);
    io::save("debug_preprocess_blur.png", basic::grayToBgr8(blurred));

    const cv::Mat eq = basic::clahe(gray);
    io::save("debug_preprocess_clahe.png", basic::grayToBgr8(eq));

    std::printf("saved debug_preprocess_*.png\n");
    return 0;
}

}  // namespace la::debug
