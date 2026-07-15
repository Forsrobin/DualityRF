#include "dsp/SpectrumAccumulator.h"

#include <cmath>

namespace duality {

float powerToDb(double power)
{
    constexpr double kFloor = 1e-12; // -120 dB
    return static_cast<float>(10.0 * std::log10(std::max(power, kFloor)));
}

SpectrumAccumulator::SpectrumAccumulator(int fftSize, WindowType window)
    : m_fft(fftSize)
    , m_window(makeWindow(window, fftSize))
    , m_windowed(fftSize)
    , m_spectrum(fftSize)
    , m_accum(fftSize, 0.0)
{
    m_pending.reserve(fftSize);
}

void SpectrumAccumulator::reset()
{
    std::fill(m_accum.begin(), m_accum.end(), 0.0);
    m_pending.clear();
    m_blocks = 0;
}

void SpectrumAccumulator::process(std::span<const Complex> samples)
{
    const std::size_t n = static_cast<std::size_t>(m_fft.size());
    std::size_t pos = 0;

    if (!m_pending.empty()) {
        const std::size_t need =
            std::min(n - m_pending.size(), samples.size());
        m_pending.insert(m_pending.end(), samples.begin(),
                         samples.begin() + need);
        pos = need;
        if (m_pending.size() < n)
            return;
        processBlock(m_pending);
        m_pending.clear();
    }

    while (samples.size() - pos >= n) {
        processBlock(samples.subspan(pos, n));
        pos += n;
    }
    m_pending.assign(samples.begin() + pos, samples.end());
}

void SpectrumAccumulator::processBlock(std::span<const Complex> block)
{
    const std::size_t n = static_cast<std::size_t>(m_fft.size());
    for (std::size_t i = 0; i < n; ++i)
        m_windowed[i] = block[i] * m_window[i];
    m_fft.execute(m_windowed, m_spectrum);

    const double norm = 1.0 / (static_cast<double>(n) * n);
    for (std::size_t i = 0; i < n; ++i)
        m_accum[i] += std::norm(m_spectrum[i]) * norm;
    ++m_blocks;
}

QVector<double> SpectrumAccumulator::linearAverage() const
{
    const int n = m_fft.size();
    QVector<double> out(n, 0.0);
    if (m_blocks == 0)
        return out;
    const double scale = 1.0 / static_cast<double>(m_blocks);
    // fftshift: negative frequencies first.
    for (int i = 0; i < n; ++i)
        out[i] = m_accum[(i + n / 2) % n] * scale;
    return out;
}

QVector<float> SpectrumAccumulator::averageDb() const
{
    const QVector<double> lin = linearAverage();
    QVector<float> out(lin.size());
    for (int i = 0; i < lin.size(); ++i)
        out[i] = powerToDb(lin[i]);
    return out;
}

} // namespace duality
