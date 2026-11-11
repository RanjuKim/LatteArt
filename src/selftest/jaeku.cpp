// Owner: Jaeku Kang - personal sandbox, edit freely.
// Run: LatteArt.exe --selftest jaeku photo.jpg
#include <cstdio>

#include "basic/color.h"
#include "basic/edge.h"
#include "io/io.h"
#include "selftest/selftest.h"

namespace la::selftest {

int jaeku(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::printf("usage: --selftest jaeku <photo.jpg>\n");
        return 1;
    }
    // Example: edge map of a raw photo (first step of cup detection).
    const cv::Mat bgr = io::loadColor(args[0]);
    const cv::Mat gray = basic::bgrToGray(bgr);
    const cv::Mat edges = basic::canny(gray, 0.05f, 0.15f);
    io::save("selftest_jaeku_edges.png", edges);
    std::printf("saved selftest_jaeku_edges.png\n");
    return 0;
}

}  // namespace la::selftest
