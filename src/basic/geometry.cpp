// Owner: Jaeku Kang
#include "basic/geometry.h"

#include <cmath>

#include <algorithm>
#include <stdexcept>

namespace la::basic {

cv::Mat warpPerspective(const cv::Mat &src, const Mat3 &dstToSrc, cv::Size dstSize) {
    if (src.empty() || (src.type() != CV_32FC1 && src.type() != CV_8UC3) || dstSize.width <= 0 ||
        dstSize.height <= 0)
        throw std::invalid_argument("warpPerspective: invalid image type or dimensions");
    for (double a : dstToSrc)
        if (!std::isfinite(a))
            throw std::invalid_argument("warpPerspective: nonfinite matrix");
    cv::Mat out(dstSize.height, dstSize.width, src.type());
    const int channels = src.type() == CV_8UC3 ? 3 : 1;
    for (int y = 0; y < out.rows; ++y)
        for (int x = 0; x < out.cols; ++x) {
            const auto &m = dstToSrc;
            double z = m[6] * x + m[7] * y + m[8];
            double sx = 0, sy = 0;
            bool valid = std::abs(z) > 1e-12;
            if (valid) {
                sx = (m[0] * x + m[1] * y + m[2]) / z;
                sy = (m[3] * x + m[4] * y + m[5]) / z;
            }
            valid = valid && std::isfinite(sx) && std::isfinite(sy) && sx >= 0 && sy >= 0 &&
                    sx <= src.cols - 1 && sy <= src.rows - 1;
            for (int c = 0; c < channels; ++c) {
                double v = 0;
                if (valid) {
                    int x0 = int(std::floor(sx)), y0 = int(std::floor(sy));
                    int x1 = std::min(x0 + 1, src.cols - 1), y1 = std::min(y0 + 1, src.rows - 1);
                    double fx = sx - x0, fy = sy - y0;
                    auto at = [&](int xx, int yy) -> double {
                        return channels == 3 ? src.ptr<uchar>(yy)[3 * xx + c]
                                             : src.ptr<float>(yy)[xx];
                    };
                    v = (1 - fy) * ((1 - fx) * at(x0, y0) + fx * at(x1, y0)) +
                        fy * ((1 - fx) * at(x0, y1) + fx * at(x1, y1));
                }
                if (channels == 3)
                    out.ptr<uchar>(y)[3 * x + c] =
                        static_cast<uchar>(std::clamp(v, 0.0, 255.0) + 0.5);
                else
                    out.ptr<float>(y)[x] = static_cast<float>(v);
            }
        }
    return out;
}

Mat3 identity() { return {1, 0, 0, 0, 1, 0, 0, 0, 1}; }

Mat3 multiply(const Mat3 &A, const Mat3 &B) {
    Mat3 C{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            for (int k = 0; k < 3; ++k)
                C[r * 3 + c] += A[r * 3 + k] * B[k * 3 + c];
    return C;
}

Mat3 inverse(const Mat3 &m) {
    const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6]) +
                       m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (!std::isfinite(det) || std::abs(det) < 1e-12)
        throw std::invalid_argument("inverse: singular/nonfinite matrix");
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

} // namespace la::basic
