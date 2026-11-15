// Owner: Junkyu Kim
#include "basic/clahe.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace la::basic {

    cv::Mat clahe(const cv::Mat& src, int tilesX,
        int tilesY, float clipLimit) {
        if (src.empty() || src.type() != CV_32FC1) {
            throw std::invalid_argument(
                "clahe: expected a non-empty CV_32FC1 image");
        }

        if (tilesX <= 0 || tilesY <= 0) {
            throw std::invalid_argument(
                "clahe: tile counts must be positive");
        }

        if (!std::isfinite(clipLimit) || clipLimit <= 0.f) {
            throw std::invalid_argument(
                "clahe: clipLimit must be finite and positive");
        }

        // Avoid empty tiles for small images.
        tilesX = std::min(tilesX, src.cols);
        tilesY = std::min(tilesY, src.rows);

        constexpr int bins = 256;
        using Lut = std::array<float, bins>;

        const std::size_t tileCount =
            static_cast<std::size_t>(tilesX) *
            static_cast<std::size_t>(tilesY);

        std::vector<Lut> luts(tileCount);
        std::vector<double> centersX(tilesX);
        std::vector<double> centersY(tilesY);

        // Divide the image without discarding pixels at its edges.
        const auto boundary = [](int index, int length, int count) {
            return static_cast<int>(
                static_cast<long long>(index) * length / count);
            };

        const auto toBin = [](float value) {
            if (!std::isfinite(value)) {
                throw std::invalid_argument(
                    "clahe: input contains a non-finite value");
            }

            const float clamped = std::clamp(value, 0.f, 1.f);
            return static_cast<int>(clamped * 255.f + 0.5f);
            };

        for (int tx = 0; tx < tilesX; ++tx) {
            const int x0 = boundary(tx, src.cols, tilesX);
            const int x1 = boundary(tx + 1, src.cols, tilesX);
            centersX[tx] = (static_cast<double>(x0) + x1 - 1) * 0.5;
        }

        for (int ty = 0; ty < tilesY; ++ty) {
            const int y0 = boundary(ty, src.rows, tilesY);
            const int y1 = boundary(ty + 1, src.rows, tilesY);
            centersY[ty] = (static_cast<double>(y0) + y1 - 1) * 0.5;
        }

        // Build a clipped histogram and cumulative mapping for each tile.
        for (int ty = 0; ty < tilesY; ++ty) {
            const int y0 = boundary(ty, src.rows, tilesY);
            const int y1 = boundary(ty + 1, src.rows, tilesY);

            for (int tx = 0; tx < tilesX; ++tx) {
                const int x0 = boundary(tx, src.cols, tilesX);
                const int x1 = boundary(tx + 1, src.cols, tilesX);

                const long long area =
                    static_cast<long long>(x1 - x0) * (y1 - y0);

                std::array<long long, bins> histogram{};

                for (int y = y0; y < y1; ++y) {
                    const float* row = src.ptr<float>(y);

                    for (int x = x0; x < x1; ++x) {
                        ++histogram[toBin(row[x])];
                    }
                }

                // clipLimit is relative to the average bin population.
                // A minimum of one keeps the limit meaningful for tiny tiles.
                const double requestedLimit =
                    static_cast<double>(clipLimit) *
                    static_cast<double>(area) / bins;

                const long long limit = static_cast<long long>(
                    std::clamp(requestedLimit, 1.0,
                        static_cast<double>(area)));

                long long excess = 0;

                for (int i = 0; i < bins; ++i) {
                    if (histogram[i] > limit) {
                        excess += histogram[i] - limit;
                        histogram[i] = limit;
                    }
                }

                // Redistribute clipped counts while preserving total population.
                const long long share = excess / bins;
                const int remainder = static_cast<int>(excess % bins);

                for (int i = 0; i < bins; ++i) {
                    histogram[i] += share;
                }

                // Spread remaining counts across the histogram.
                for (int i = 0; i < remainder; ++i) {
                    const int index = i * bins / remainder;
                    ++histogram[index];
                }

                Lut& lut = luts[
                    static_cast<std::size_t>(ty) * tilesX + tx];

                long long cumulative = 0;

                for (int i = 0; i < bins; ++i) {
                    cumulative += histogram[i];
                    lut[i] = static_cast<float>(
                        static_cast<double>(cumulative) /
                        static_cast<double>(area));
                }
            }
        }

        struct Blend {
            int low;
            int high;
            float weight;
        };

        // Interpolate between actual tile centers.
        // Outside the outermost centers, use the nearest tile.
        const auto findBlend =
            [](int position, const std::vector<double>& centers) -> Blend {
            if (position <= centers.front()) {
                return { 0, 0, 0.f };
            }

            const int last = static_cast<int>(centers.size()) - 1;

            if (position >= centers.back()) {
                return { last, last, 0.f };
            }

            const auto next = std::upper_bound(
                centers.begin(), centers.end(),
                static_cast<double>(position));

            const int high =
                static_cast<int>(next - centers.begin());
            const int low = high - 1;

            const float weight = static_cast<float>(
                (position - centers[low]) /
                (centers[high] - centers[low]));

            return { low, high, weight };
            };

        std::vector<Blend> blendsX(src.cols);
        std::vector<Blend> blendsY(src.rows);

        for (int x = 0; x < src.cols; ++x) {
            blendsX[x] = findBlend(x, centersX);
        }

        for (int y = 0; y < src.rows; ++y) {
            blendsY[y] = findBlend(y, centersY);
        }

        cv::Mat dst(src.rows, src.cols, CV_32FC1);

        for (int y = 0; y < src.rows; ++y) {
            const float* input = src.ptr<float>(y);
            float* output = dst.ptr<float>(y);
            const Blend by = blendsY[y];

            for (int x = 0; x < src.cols; ++x) {
                const Blend bx = blendsX[x];
                const int bin = toBin(input[x]);

                const float topLeft = luts[
                    static_cast<std::size_t>(by.low) * tilesX +
                        bx.low][bin];

                    const float topRight = luts[
                        static_cast<std::size_t>(by.low) * tilesX +
                            bx.high][bin];

                        const float bottomLeft = luts[
                            static_cast<std::size_t>(by.high) * tilesX +
                                bx.low][bin];

                            const float bottomRight = luts[
                                static_cast<std::size_t>(by.high) * tilesX +
                                    bx.high][bin];

                                const float top =
                                    topLeft + bx.weight * (topRight - topLeft);

                                const float bottom =
                                    bottomLeft + bx.weight * (bottomRight - bottomLeft);

                                output[x] = std::clamp(
                                    top + by.weight * (bottom - top), 0.f, 1.f);
            }
        }

        return dst;
    }

}  // namespace la::basic