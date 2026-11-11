// Owner: Jaeku Kang
#include "basic/geometry.h"

#include <cmath>

#include "core/todo.h"

namespace la::basic {

cv::Mat warpPerspective(const cv::Mat& src, const Mat3& dstToSrc, cv::Size dstSize) {
    (void)src; (void)dstToSrc; (void)dstSize;
    LA_TODO("warpPerspective: inverse mapping + bilinear interpolation");
}

Mat3 identity() { return {1, 0, 0, 0, 1, 0, 0, 0, 1}; }

Mat3 multiply(const Mat3& A, const Mat3& B) {
    Mat3 C{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            for (int k = 0; k < 3; ++k) C[r * 3 + c] += A[r * 3 + k] * B[k * 3 + c];
    return C;
}

Mat3 inverse(const Mat3& m) {
    const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
                       m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (std::abs(det) < 1e-12) LA_TODO("inverse: singular matrix handling");
    const double s = 1.0 / det;
    return {(m[4] * m[8] - m[5] * m[7]) * s, (m[2] * m[7] - m[1] * m[8]) * s,
            (m[1] * m[5] - m[2] * m[4]) * s, (m[5] * m[6] - m[3] * m[8]) * s,
            (m[0] * m[8] - m[2] * m[6]) * s, (m[2] * m[3] - m[0] * m[5]) * s,
            (m[3] * m[7] - m[4] * m[6]) * s, (m[1] * m[6] - m[0] * m[7]) * s,
            (m[0] * m[4] - m[1] * m[3]) * s};
}

Mat3 translation(double tx, double ty) { return {1, 0, tx, 0, 1, ty, 0, 0, 1}; }

Mat3 rotationAbout(cv::Point2f c, double angleDeg) {
    const double t = angleDeg * M_PI / 180.0, cs = std::cos(t), sn = std::sin(t);
    const Mat3 R{cs, -sn, 0, sn, cs, 0, 0, 0, 1};
    return multiply(translation(c.x, c.y), multiply(R, translation(-c.x, -c.y)));
}

Mat3 scaleAbout(cv::Point2f c, double sx, double sy) {
    const Mat3 S{sx, 0, 0, 0, sy, 0, 0, 0, 1};
    return multiply(translation(c.x, c.y), multiply(S, translation(-c.x, -c.y)));
}

}  // namespace la::basic
