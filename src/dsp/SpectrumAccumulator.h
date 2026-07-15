#pragma once

#include "core/Types.h"
#include "dsp/FftEngine.h"
#include "dsp/Window.h"

#include <QVector>

#include <span>
#include <vector>

namespace duality {

// Offline average power spectrum: feed arbitrary-sized chunks (e.g. while
// streaming a recording off disk), then read the accumulated result.
// Outputs are fftshifted so index 0 = -sampleRate/2.
class SpectrumAccumulator {
public:
    explicit SpectrumAccumulator(int fftSize, WindowType window = WindowType::Hann);

    void process(std::span<const Complex> samples);
    void reset();

    int fftSize() const { return m_fft.size(); }
    std::size_t blocks() const { return m_blocks; }

    // Mean linear power per bin (|X|²/N², full-scale CW ≈ 1.0).
    QVector<double> linearAverage() const;
    QVector<float> averageDb() const;

private:
    void processBlock(std::span<const Complex> block);

    FftEngine m_fft;
    std::vector<float> m_window;
    std::vector<Complex> m_pending;
    std::vector<Complex> m_windowed;
    std::vector<Complex> m_spectrum;
    std::vector<double> m_accum;
    std::size_t m_blocks = 0;
};

// dB conversion used across the app: 10·log10(power) with a floor.
float powerToDb(double power);

} // namespace duality
