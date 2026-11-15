// Owner: Ranju Kim
#include "basic/fft.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace la::basic {

namespace {

bool isPowerOfTwo(size_t n) { return n > 0 && (n & (n - 1)) == 0; }

size_t nextPowerOfTwo(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

}  // namespace

// Iterative Cooley-Tukey radix-2 FFT.
//   forward: X[k] = sum_n x[n] e^{-2 pi i k n / N}
//   inverse: x[n] = (1/N) sum_k X[k] e^{+2 pi i k n / N}
void fft(std::vector<Complex>& a, bool inverse) {
    const size_t n = a.size();
    if (!isPowerOfTwo(n)) throw std::invalid_argument("fft: size must be a power of two");

    // Bit-reversal permutation.
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    // Butterflies over blocks of length 2, 4, ..., n.
    for (size_t len = 2; len <= n; len <<= 1) {
        const double angle = 2.0 * M_PI / static_cast<double>(len) * (inverse ? 1.0 : -1.0);
        const Complex wLen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            Complex w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const Complex u = a[i + k];
                const Complex v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wLen;
            }
        }
    }

    if (inverse)
        for (Complex& x : a) x /= static_cast<double>(n);
}

// Note: the DC term |X[0]| is included. Subtract the mean from the signal
// first if only the periodic part (e.g. layer spacing) is of interest.
std::vector<double> magnitudeSpectrum(const std::vector<float>& signal) {
    if (signal.empty()) throw std::invalid_argument("magnitudeSpectrum: empty signal");
    const size_t n = nextPowerOfTwo(signal.size());
    std::vector<Complex> a(n, Complex(0.0, 0.0));
    for (size_t i = 0; i < signal.size(); ++i) a[i] = Complex(signal[i], 0.0);
    fft(a);

    std::vector<double> mag(n / 2 + 1);
    for (size_t k = 0; k < mag.size(); ++k) mag[k] = std::abs(a[k]);
    return mag;
}

// Rows first, then columns. The output has the padded size (powers of two)
// and is NOT shifted: the DC term is at (0, 0), and frequency (u, v) and
// (N-u, M-v) are mirror images. Distance from the nearest corner = frequency.
cv::Mat fft2Magnitude(const cv::Mat& src) {
    if (src.empty() || src.type() != CV_32FC1)
        throw std::invalid_argument("fft2Magnitude: expected a non-empty CV_32FC1 image");

    const size_t rows = nextPowerOfTwo(static_cast<size_t>(src.rows));
    const size_t cols = nextPowerOfTwo(static_cast<size_t>(src.cols));
    std::vector<Complex> data(rows * cols, Complex(0.0, 0.0));
    for (int y = 0; y < src.rows; ++y) {
        const float* s = src.ptr<float>(y);
        for (int x = 0; x < src.cols; ++x) data[y * cols + x] = Complex(s[x], 0.0);
    }

    std::vector<Complex> line(cols);
    for (size_t y = 0; y < rows; ++y) {
        for (size_t x = 0; x < cols; ++x) line[x] = data[y * cols + x];
        fft(line);
        for (size_t x = 0; x < cols; ++x) data[y * cols + x] = line[x];
    }
    line.assign(rows, Complex(0.0, 0.0));
    for (size_t x = 0; x < cols; ++x) {
        for (size_t y = 0; y < rows; ++y) line[y] = data[y * cols + x];
        fft(line);
        for (size_t y = 0; y < rows; ++y) data[y * cols + x] = line[y];
    }

    cv::Mat mag(static_cast<int>(rows), static_cast<int>(cols), CV_32FC1);
    for (size_t y = 0; y < rows; ++y) {
        float* d = mag.ptr<float>(static_cast<int>(y));
        for (size_t x = 0; x < cols; ++x) d[x] = static_cast<float>(std::abs(data[y * cols + x]));
    }
    return mag;
}

}  // namespace la::basic
