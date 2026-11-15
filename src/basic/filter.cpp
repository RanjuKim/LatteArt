// Owner: Junkyu Kim
#include "basic/filter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace la::basic {

    // Generic mathematical convolution.
    // The kernel is flipped horizontally and vertically.
    // Border handling: replicate the nearest edge pixel.
    cv::Mat convolve2D(const cv::Mat& src, const cv::Mat& kernel) {
        if (src.empty() || src.type() != CV_32FC1) {
            throw std::invalid_argument(
                "convolve2D: src must be a non-empty CV_32FC1 image");
        }

        if (kernel.empty() || kernel.type() != CV_32FC1 ||
            kernel.rows % 2 == 0 || kernel.cols % 2 == 0) {
            throw std::invalid_argument(
                "convolve2D: kernel must be non-empty CV_32FC1 with odd dimensions");
        }

        cv::Mat dst(src.rows, src.cols, CV_32FC1);

        const int radiusY = kernel.rows / 2;
        const int radiusX = kernel.cols / 2;

        for (int y = 0; y < src.rows; ++y) {
            float* out = dst.ptr<float>(y);

            for (int x = 0; x < src.cols; ++x) {
                double sum = 0.0;

                for (int ky = 0; ky < kernel.rows; ++ky) {
                    // Use a wider integer type before clamping coordinates.
                    const int sy = static_cast<int>(std::clamp(
                        static_cast<long long>(y) + radiusY - ky,
                        0LL, static_cast<long long>(src.rows - 1)));

                    const float* inputRow = src.ptr<float>(sy);
                    const float* kernelRow = kernel.ptr<float>(ky);

                    for (int kx = 0; kx < kernel.cols; ++kx) {
                        const int sx = static_cast<int>(std::clamp(
                            static_cast<long long>(x) + radiusX - kx,
                            0LL, static_cast<long long>(src.cols - 1)));

                        sum += static_cast<double>(inputRow[sx])
                            * kernelRow[kx];
                    }
                }

                // Preserve negative values and values above 1 for derivative filters.
                out[x] = static_cast<float>(sum);
            }
        }

        return dst;
    }

    cv::Mat gaussianBlur(const cv::Mat& src, float sigma) {
        if (src.empty() || src.type() != CV_32FC1) {
            throw std::invalid_argument(
                "gaussianBlur: src must be a non-empty CV_32FC1 image");
        }

        if (!std::isfinite(sigma) || sigma <= 0.0f) {
            throw std::invalid_argument(
                "gaussianBlur: sigma must be finite and greater than zero");
        }

        // Keep approximately three standard deviations on either side.
        const double radiusValue =
            std::ceil(3.0 * static_cast<double>(sigma));

        if (radiusValue >
            static_cast<double>((std::numeric_limits<int>::max() - 1) / 2)) {
            throw std::invalid_argument("gaussianBlur: sigma is too large");
        }

        const int radius = static_cast<int>(radiusValue);
        const int size = 2 * radius + 1;

        cv::Mat horizontal(1, size, CV_32FC1);
        cv::Mat vertical(size, 1, CV_32FC1);

        const double sigmaSquared =
            static_cast<double>(sigma) * static_cast<double>(sigma);

        double weightSum = 0.0;
        float* weights = horizontal.ptr<float>(0);

        for (int i = 0; i < size; ++i) {
            const double offset = static_cast<double>(i - radius);
            const double weight =
                std::exp(-(offset * offset) / (2.0 * sigmaSquared));

            weights[i] = static_cast<float>(weight);
            weightSum += weight;
        }

        // Normalize so a constant image retains its brightness.
        for (int i = 0; i < size; ++i) {
            const float normalized =
                static_cast<float>(weights[i] / weightSum);

            weights[i] = normalized;
            vertical.ptr<float>(i)[0] = normalized;
        }

        const cv::Mat intermediate = convolve2D(src, horizontal);
        return convolve2D(intermediate, vertical);
    }

}  // namespace la::basic