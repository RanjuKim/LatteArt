#pragma once
#include <complex>
#include <opencv2/core.hpp>
#include <vector>

namespace la::basic {

using Complex = std::complex<double>;

// In-place radix-2 FFT. a.size() must be a power of two.
void fft(std::vector<Complex>& a, bool inverse = false);

// Zero-pads to the next power of two and returns |X[k]| for k = 0..N/2.
std::vector<double> magnitudeSpectrum(const std::vector<float>& signal);

// 2D FFT magnitude of CV_32FC1 (rows then columns), size padded to 2^k.
cv::Mat fft2Magnitude(const cv::Mat& src);

}  // namespace la::basic
