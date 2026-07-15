#pragma once

#include <QVector>

namespace duality {

struct SpectrumStats {
    double occupiedBandwidthHz = 0.0; // 99% power bandwidth
    double peakDb = -120.0;
    double meanDb = -120.0;
    double peakOffsetHz = 0.0; // peak bin offset from center frequency
};

// `linearShifted` is a mean linear power spectrum, fftshifted (index 0 =
// -sampleRate/2), as produced by SpectrumAccumulator::linearAverage().
SpectrumStats analyzeSpectrum(const QVector<double> &linearShifted,
                              double sampleRateHz, double powerFraction = 0.99);

} // namespace duality
