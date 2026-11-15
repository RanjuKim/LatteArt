// Owner: Ranju Kim  (1D fft also used by Jaeku for the layer cross-check; deliver by Nov 17)
#pragma once
#include <complex>
#include <opencv2/core.hpp>
#include <vector>

namespace la::basic {

using Complex = std::complex<double>;

// In-place radix-2 FFT. a.size() must be a power of two.
void fft(std::vector<Complex>& a, bool inverse = false);

// Zero-pads the signal to Npad = next power of two >= signal.size() and
// returns |X[k]| for k = 0..Npad/2. Npad is written to *paddedSize if given.
//
// IMPORTANT for counting repetitions (e.g. layers along a profile):
//   bin k = k / Npad cycles per sample
//         = k * signal.size() / Npad cycles over the whole signal.
//   Example: 200 samples with 4 layers -> peak near k = 4 * 256 / 200 = 5.1,
//   so convert: cycles = k * signal.size() / Npad, do NOT use k directly.
// Subtract the signal mean first: otherwise |X[0]| dominates and the jump to
// the zero padding adds leakage.
std::vector<double> magnitudeSpectrum(const std::vector<float>& signal,
                                      size_t* paddedSize = nullptr);

// 2D FFT magnitude of CV_32FC1 (rows then columns), size padded to 2^k.
cv::Mat fft2Magnitude(const cv::Mat& src);

}  // namespace la::basic
