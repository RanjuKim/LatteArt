// Owner: Junkyu Kim
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "basic/clahe.h"
#include "basic/color.h"
#include "basic/filter.h"
#include "debug/debug.h"
#include "io/io.h"
#include "viz/report.h"

namespace la::debug {

    static void expectNear(float actual, float expected) {
        if (!std::isfinite(actual) ||
            std::fabs(actual - expected) > 0.00001f) {
            std::printf(
                "[FAIL] expected %.6f, got %.6f\n",
                expected, actual);
            throw std::runtime_error("filter test failed");
        }
    }

    static void runFilterTests() {
        // Test 1: identity kernel preserves every input pixel.
        cv::Mat src(2, 3, CV_32FC1);
        src.ptr<float>(0)[0] = -1.f;
        src.ptr<float>(0)[1] = 2.f;
        src.ptr<float>(0)[2] = 4.f;
        src.ptr<float>(1)[0] = 8.f;
        src.ptr<float>(1)[1] = 0.f;
        src.ptr<float>(1)[2] = 3.f;

        cv::Mat identity(1, 1, CV_32FC1);
        identity.ptr<float>(0)[0] = 1.f;

        const cv::Mat same = basic::convolve2D(src, identity);

        if (same.rows != src.rows || same.cols != src.cols ||
            same.type() != CV_32FC1) {
            throw std::runtime_error("filter output format mismatch");
        }

        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                expectNear(same.ptr<float>(y)[x],
                    src.ptr<float>(y)[x]);
            }
        }
        std::printf("[PASS] identity convolution\n");

        // Test 2: asymmetric kernel checks true convolution and borders.
        // Input: [1, 2, 4], kernel: [1, 0, -1]
        // With replicate borders, output must be [1, 3, 2].
        cv::Mat row(1, 3, CV_32FC1);
        row.ptr<float>(0)[0] = 1.f;
        row.ptr<float>(0)[1] = 2.f;
        row.ptr<float>(0)[2] = 4.f;

        cv::Mat kernel(1, 3, CV_32FC1);
        kernel.ptr<float>(0)[0] = 1.f;
        kernel.ptr<float>(0)[1] = 0.f;
        kernel.ptr<float>(0)[2] = -1.f;

        const cv::Mat horizontal = basic::convolve2D(row, kernel);
        expectNear(horizontal.ptr<float>(0)[0], 1.f);
        expectNear(horizontal.ptr<float>(0)[1], 3.f);
        expectNear(horizontal.ptr<float>(0)[2], 2.f);
        std::printf("[PASS] horizontal convolution and borders\n");

        // Test 3: verify the same calculation in the vertical direction.
        cv::Mat column(3, 1, CV_32FC1);
        cv::Mat verticalKernel(3, 1, CV_32FC1);

        for (int i = 0; i < 3; ++i) {
            column.ptr<float>(i)[0] = row.ptr<float>(0)[i];
            verticalKernel.ptr<float>(i)[0] = kernel.ptr<float>(0)[i];
        }

        const cv::Mat vertical =
            basic::convolve2D(column, verticalKernel);

        expectNear(vertical.ptr<float>(0)[0], 1.f);
        expectNear(vertical.ptr<float>(1)[0], 3.f);
        expectNear(vertical.ptr<float>(2)[0], 2.f);
        std::printf("[PASS] vertical convolution and borders\n");

        // Test 4: Gaussian blur preserves a constant image,
        // even when its kernel is larger than the image.
        cv::Mat constant(3, 4, CV_32FC1);
        for (int y = 0; y < constant.rows; ++y) {
            for (int x = 0; x < constant.cols; ++x) {
                constant.ptr<float>(y)[x] = 0.4f;
            }
        }

        const cv::Mat blurred = basic::gaussianBlur(constant, 2.f);

        if (blurred.rows != constant.rows ||
            blurred.cols != constant.cols ||
            blurred.type() != CV_32FC1) {
            throw std::runtime_error("Gaussian output format mismatch");
        }

        for (int y = 0; y < blurred.rows; ++y) {
            for (int x = 0; x < blurred.cols; ++x) {
                expectNear(blurred.ptr<float>(y)[x], 0.4f);
            }
        }
        std::printf("[PASS] Gaussian constant image and borders\n");
        std::printf("All filter tests passed.\n");
    }

    static void runLabTests() {
        // Each pixel is stored in B, G, R order.
        const unsigned char colors[6][3] = {
            {0,   0,   0},    // black
            {255, 255, 255},  // white
            {128, 128, 128},  // gray
            {0,   0,   255},  // red
            {0,   255, 0},    // green
            {255, 0,   0}     // blue
        };

        const char* names[6] = {
            "black", "white", "gray", "red", "green", "blue"
        };

        // Approximate sRGB -> CIE Lab values, D65 reference white.
        const float expected[6][3] = {
            {0.000f,   0.000f,    0.000f},
            {100.000f, 0.000f,    0.000f},
            {53.585f,  0.000f,    0.000f},
            {53.241f,  80.092f,   67.203f},
            {87.735f, -86.183f,   83.179f},
            {32.297f,  79.188f, -107.860f}
        };

        cv::Mat input(1, 6, CV_8UC3);
        unsigned char* pixels = input.ptr<unsigned char>(0);

        for (int x = 0; x < 6; ++x) {
            for (int c = 0; c < 3; ++c) {
                pixels[3 * x + c] = colors[x][c];
            }
        }

        cv::Mat L, a, b;
        basic::bgrToLab(input, L, a, b);

        const cv::Mat* channels[3] = { &L, &a, &b };

        for (int c = 0; c < 3; ++c) {
            if (channels[c]->rows != 1 ||
                channels[c]->cols != 6 ||
                channels[c]->type() != CV_32FC1) {
                throw std::runtime_error("Lab output format mismatch");
            }
        }

        for (int x = 0; x < 6; ++x) {
            for (int c = 0; c < 3; ++c) {
                const float actual = channels[c]->ptr<float>(0)[x];

                // Allow small rounding differences in reference values.
                if (!std::isfinite(actual) ||
                    std::fabs(actual - expected[x][c]) > 0.03f) {
                    std::printf(
                        "[FAIL] Lab %s channel %d: expected %.3f, got %.3f\n",
                        names[x], c, expected[x][c], actual);
                    throw std::runtime_error("Lab test failed");
                }
            }

            std::printf(
                "[PASS] Lab %s: L=%.3f a=%.3f b=%.3f\n",
                names[x],
                L.ptr<float>(0)[x],
                a.ptr<float>(0)[x],
                b.ptr<float>(0)[x]);
        }

        std::printf("All Lab tests passed.\n");
    }

    static void runClaheTests() {
        // Test 1: one tile with four equally populated brightness bins.
        // A sufficiently high clip limit disables clipping for this input.
        cv::Mat ramp(1, 4, CV_32FC1);
        ramp.ptr<float>(0)[0] = 0.f;
        ramp.ptr<float>(0)[1] = 85.f / 255.f;
        ramp.ptr<float>(0)[2] = 170.f / 255.f;
        ramp.ptr<float>(0)[3] = 1.f;

        const cv::Mat mapped = basic::clahe(ramp, 1, 1, 256.f);

        expectNear(mapped.ptr<float>(0)[0], 0.25f);
        expectNear(mapped.ptr<float>(0)[1], 0.50f);
        expectNear(mapped.ptr<float>(0)[2], 0.75f);
        expectNear(mapped.ptr<float>(0)[3], 1.00f);
        std::printf("[PASS] CLAHE cumulative histogram\n");

        // Test 2: clipping and redistribution on a single tile.
        // 256 black pixels, clipLimit=1 -> bin limit=1.
        // Excess 255 counts are redistributed to bins 0..254.
        // Therefore bin 0 has 2 counts and maps to 2/256.
        cv::Mat black(16, 16, CV_32FC1);
        for (int y = 0; y < black.rows; ++y) {
            for (int x = 0; x < black.cols; ++x) {
                black.ptr<float>(y)[x] = 0.f;
            }
        }

        const cv::Mat clipped = basic::clahe(black, 1, 1, 1.f);

        for (int y = 0; y < clipped.rows; ++y) {
            for (int x = 0; x < clipped.cols; ++x) {
                expectNear(clipped.ptr<float>(y)[x], 2.f / 256.f);
            }
        }
        std::printf("[PASS] CLAHE clipping and redistribution\n");

        // Test 3: uneven tile dimensions and output range.
        cv::Mat image(7, 11, CV_32FC1);
        for (int y = 0; y < image.rows; ++y) {
            for (int x = 0; x < image.cols; ++x) {
                image.ptr<float>(y)[x] =
                    static_cast<float>(y * image.cols + x) /
                    static_cast<float>(image.rows * image.cols - 1);
            }
        }

        const cv::Mat result = basic::clahe(image, 3, 2, 2.f);

        if (result.rows != image.rows ||
            result.cols != image.cols ||
            result.type() != CV_32FC1) {
            throw std::runtime_error("CLAHE output format mismatch");
        }

        for (int y = 0; y < result.rows; ++y) {
            for (int x = 0; x < result.cols; ++x) {
                const float value = result.ptr<float>(y)[x];

                if (!std::isfinite(value) || value < 0.f || value > 1.f) {
                    throw std::runtime_error("CLAHE output outside 0..1");
                }
            }
        }
        std::printf("[PASS] CLAHE uneven tiles and output range\n");

        // Test 4: default tile counts must also handle a 1x1 image.
        cv::Mat tiny(1, 1, CV_32FC1);
        tiny.ptr<float>(0)[0] = 0.5f;

        const cv::Mat tinyResult = basic::clahe(tiny);

        if (tinyResult.rows != 1 || tinyResult.cols != 1 ||
            tinyResult.type() != CV_32FC1) {
            throw std::runtime_error("CLAHE tiny output format mismatch");
        }

        expectNear(tinyResult.ptr<float>(0)[0], 1.f);
        std::printf("[PASS] CLAHE tiny image\n");

        std::printf("All CLAHE tests passed.\n");
    }

    static void previewReport() {
        PatternSpec spec;
        spec.name = "tulip";
        spec.targetLayers = 3;

        LayerResult reference;
        reference.count = 3;
        reference.fftCount = 3;

        LayerResult user;
        user.count = 3;
        user.fftCount = 3;
        user.spacingCV = 0.12f;
        user.thicknessCV = 0.18f;
        user.U = 0.80f;

        QualityResult quality;
        quality.contrast = {
            0.70f, 0.80f, 0.90f, "Contrast is close to the reference."
        };
        quality.shape = {
            0.75f, 0.90f, 0.80f, "Improve left-right symmetry."
        };
        quality.texture = {
            0.70f, 0.85f, 0.85f, "Some surface bubbles are visible."
        };
        quality.position = {
            0.08f, 0.02f, 0.90f, "Move the pattern closer to the center."
        };
        quality.size = {
            0.30f, 0.35f, 0.80f, "The pattern is smaller than the reference."
        };
        quality.Q = 0.855f;

        ScoreResult score;
        score.deltaN = 0;
        score.bandLow = 60.f;
        score.bandHigh = 100.f;
        score.U = user.U;
        score.Q = quality.Q;
        score.finalScore = 93.54f;

        std::printf("\n[DEMO 1: synthetic scoring data]\n");
        viz::printReport(spec, reference, user, quality, score);

        // Preview the uncertain-result display as well.
        user.fftCount = 2;
        user.uncertain = true;
        score.needsReview = true;

        std::printf("\n[DEMO 2: synthetic uncertain result]\n");
        viz::printReport(spec, reference, user, quality, score);
    }

    int runPreprocess(const Args& args) {
        if (!args.empty() && args[0] == "--report") {
            previewReport();
            return 0;
        }
        runFilterTests();
        runLabTests();
        runClaheTests();

        if (args.empty()) {
            std::printf("usage: --debug preprocess <photo.jpg>\n");
            return 0;
        }

        const cv::Mat bgr = io::loadColor(args[0]);
        const cv::Mat gray = basic::bgrToGray(bgr);
        io::save("debug_preprocess_gray.png",
            basic::grayToBgr8(gray));

        const cv::Mat blurred = basic::gaussianBlur(gray, 2.f);
        io::save("debug_preprocess_blur.png",
            basic::grayToBgr8(blurred));

        const cv::Mat eq = basic::clahe(gray);
        io::save("debug_preprocess_clahe.png",
            basic::grayToBgr8(eq));

        std::printf("saved debug_preprocess_*.png\n");
        return 0;
    }

}  // namespace la::debug