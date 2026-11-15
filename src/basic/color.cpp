// Owner: Junkyu Kim
#include "basic/color.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "core/todo.h"

namespace la::basic {

// Reference implementation that shows the coding style: allocate the output
// Mat, then walk pixels with ptr<T>(row). No cv:: processing functions.
cv::Mat bgrToGray(const cv::Mat& bgr) {
    if (bgr.type() != CV_8UC3) throw std::invalid_argument("bgrToGray: expected CV_8UC3");
    cv::Mat gray(bgr.rows, bgr.cols, CV_32FC1);
    for (int y = 0; y < bgr.rows; ++y) {
        const uchar* s = bgr.ptr<uchar>(y);
        float* d = gray.ptr<float>(y);
        for (int x = 0; x < bgr.cols; ++x) {
            const uchar B = s[3 * x], G = s[3 * x + 1], R = s[3 * x + 2];
            d[x] = (0.114f * B + 0.587f * G + 0.299f * R) / 255.f;
        }
    }
    return gray;
}

void bgrToLab(const cv::Mat& bgr, cv::Mat& L, cv::Mat& a, cv::Mat& b) {
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        throw std::invalid_argument(
            "bgrToLab: expected a non-empty CV_8UC3 image");
    }

    // Separate output objects are required for the three channels.
    if (&L == &a || &L == &b || &a == &b) {
        throw std::invalid_argument(
            "bgrToLab: output channels must be different Mat objects");
    }

    // Use local outputs so the input remains valid during conversion.
    cv::Mat outL(bgr.rows, bgr.cols, CV_32FC1);
    cv::Mat outA(bgr.rows, bgr.cols, CV_32FC1);
    cv::Mat outB(bgr.rows, bgr.cols, CV_32FC1);

    // Decode the sRGB transfer function.
    const auto toLinear = [](double value) -> double {
        if (value <= 0.04045) {
            return value / 12.92;
        }
        return std::pow((value + 0.055) / 1.055, 2.4);
        };

    // CIE Lab piecewise function.
    const auto labF = [](double value) -> double {
        constexpr double delta = 6.0 / 29.0;
        constexpr double threshold = delta * delta * delta;

        if (value > threshold) {
            return std::cbrt(value);
        }

        return value / (3.0 * delta * delta) + 4.0 / 29.0;
        };

    // D65 reference white, with Y normalized to 1.
    constexpr double Xn = 0.95047;
    constexpr double Yn = 1.00000;
    constexpr double Zn = 1.08883;

    for (int y = 0; y < bgr.rows; ++y) {
        const uchar* input = bgr.ptr<uchar>(y);

        float* dstL = outL.ptr<float>(y);
        float* dstA = outA.ptr<float>(y);
        float* dstB = outB.ptr<float>(y);

        for (int x = 0; x < bgr.cols; ++x) {
            // OpenCV color images store channels in B, G, R order.
            const double blue = toLinear(input[3 * x] / 255.0);
            const double green = toLinear(input[3 * x + 1] / 255.0);
            const double red = toLinear(input[3 * x + 2] / 255.0);

            // Linear sRGB -> CIE XYZ, D65.
            const double X =
                0.4124564 * red + 0.3575761 * green + 0.1804375 * blue;
            const double Y =
                0.2126729 * red + 0.7151522 * green + 0.0721750 * blue;
            const double Z =
                0.0193339 * red + 0.1191920 * green + 0.9503041 * blue;

            const double fx = labF(X / Xn);
            const double fy = labF(Y / Yn);
            const double fz = labF(Z / Zn);

            dstL[x] = static_cast<float>(
                std::clamp(116.0 * fy - 16.0, 0.0, 100.0));
            dstA[x] = static_cast<float>(500.0 * (fx - fy));
            dstB[x] = static_cast<float>(200.0 * (fy - fz));
        }
    }

    L = outL;
    a = outA;
    b = outB;
}

cv::Mat grayToBgr8(const cv::Mat& gray) {
    if (gray.type() != CV_32FC1) throw std::invalid_argument("grayToBgr8: expected CV_32FC1");
    cv::Mat out(gray.rows, gray.cols, CV_8UC3);
    for (int y = 0; y < gray.rows; ++y) {
        const float* s = gray.ptr<float>(y);
        uchar* d = out.ptr<uchar>(y);
        for (int x = 0; x < gray.cols; ++x) {
            const uchar v = static_cast<uchar>(std::clamp(s[x], 0.f, 1.f) * 255.f + 0.5f);
            d[3 * x] = d[3 * x + 1] = d[3 * x + 2] = v;
        }
    }
    return out;
}

}  // namespace la::basic
