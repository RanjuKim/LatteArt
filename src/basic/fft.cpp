#include "basic/fft.h"

#include "core/todo.h"

namespace la::basic {

void fft(std::vector<Complex>& a, bool inverse) {
    (void)a; (void)inverse;
    LA_TODO("fft: bit-reversal permutation + butterflies");
}

std::vector<double> magnitudeSpectrum(const std::vector<float>& signal) {
    (void)signal;
    LA_TODO("magnitudeSpectrum");
}

cv::Mat fft2Magnitude(const cv::Mat& src) {
    (void)src;
    LA_TODO("fft2Magnitude");
}

}  // namespace la::basic
