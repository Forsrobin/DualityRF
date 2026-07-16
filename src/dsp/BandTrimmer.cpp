#include "dsp/BandTrimmer.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace duality {

namespace {

// Approximate main-lobe width (in normalized frequency) of a Hamming window,
// used to size the filter for a target transition band.
constexpr double kHammingTransition = 3.3;

} // namespace

BandTrimmer::BandTrimmer(double inputRateHz, double halfWidthHz)
    : m_inputRateHz(inputRateHz)
    , m_bandwidthHz(2.0 * halfWidthHz)
{
    // Nothing sensible to trim: no band set, or the band already spans (almost)
    // the whole capture. Leave the samples as-is.
    if (halfWidthHz <= 0.0 || inputRateHz <= 0.0 ||
        m_bandwidthHz >= inputRateHz * 0.95)
        return;

    // Keep the capture sample rate (m_decimation stays 1) so the trimmed file
    // is still replayable by the transmit hardware, which cannot stream at the
    // low rates a decimated band would need. Just band-limit the content to
    // ±halfWidthHz with a low-pass whose transition sits above the passband
    // edge, so everything outside the capture range is removed.
    const double transitionHz =
        std::max(0.3 * halfWidthHz, 0.002 * inputRateHz);
    double fc = (halfWidthHz + 0.5 * transitionHz) / inputRateHz;
    fc = std::min(fc, 0.49); // stay below Nyquist
    int taps = static_cast<int>(
        std::ceil(kHammingTransition * inputRateHz / transitionHz));
    taps = std::clamp(taps, 31, 1023);
    if (taps % 2 == 0)
        ++taps;

    m_taps.resize(taps);
    const int half = (taps - 1) / 2;
    const double twoPi = 2.0 * std::numbers::pi;
    double sum = 0.0;
    for (int n = 0; n < taps; ++n) {
        const int k = n - half;
        const double sinc =
            (k == 0) ? 2.0 * fc
                     : std::sin(twoPi * fc * k) / (std::numbers::pi * k);
        const double hamming =
            0.54 - 0.46 * std::cos(twoPi * n / (taps - 1));
        const double h = sinc * hamming;
        m_taps[n] = static_cast<float>(h);
        sum += h;
    }
    // Normalize to unity DC gain so signal levels are preserved.
    if (sum != 0.0)
        for (float &h : m_taps)
            h = static_cast<float>(h / sum);
}

std::vector<Complex> BandTrimmer::process(std::span<const Complex> in) const
{
    if (m_taps.empty())
        return std::vector<Complex>(in.begin(), in.end());

    const int taps = static_cast<int>(m_taps.size());
    const int half = (taps - 1) / 2;
    const int n = static_cast<int>(in.size());
    const int outN = n / m_decimation;

    std::vector<Complex> out;
    out.reserve(std::max(0, outN));
    for (int k = 0; k < outN; ++k) {
        const int center = k * m_decimation;
        Complex acc{0.0f, 0.0f};
        for (int j = 0; j < taps; ++j) {
            const int idx = center + j - half;
            if (idx >= 0 && idx < n)
                acc += m_taps[j] * in[idx];
        }
        out.push_back(acc);
    }
    return out;
}

void BandTrimmer::resetStream()
{
    m_hist.clear();
    m_baseIndex = 0;
    m_nextK = 0;
    m_skip = 0;
}

std::vector<Complex> BandTrimmer::processStream(std::span<const Complex> in)
{
    if (m_taps.empty())
        return std::vector<Complex>(in.begin(), in.end());

    const std::size_t taps = m_taps.size();
    const std::size_t dec = static_cast<std::size_t>(m_decimation);

    // Discard any input the decimation phase skipped over on a previous chunk.
    std::size_t off = 0;
    if (m_skip > 0) {
        const std::size_t drop = std::min(m_skip, in.size());
        off = drop;
        m_skip -= drop;
        m_baseIndex += drop;
    }
    m_hist.insert(m_hist.end(), in.begin() + off, in.end());

    // A causal FIR: output k uses input window [k*dec, k*dec + taps). Emit
    // every output whose full window has arrived. The group delay (half the
    // filter) is harmless for storage/replay.
    std::vector<Complex> out;
    while (true) {
        const std::size_t s = m_nextK * dec;
        if (s + taps > m_baseIndex + m_hist.size())
            break;
        const std::size_t local = s - m_baseIndex;
        Complex acc{0.0f, 0.0f};
        for (std::size_t j = 0; j < taps; ++j)
            acc += m_taps[j] * m_hist[local + j];
        out.push_back(acc);
        ++m_nextK;
    }

    // Drop consumed history; if decimation stepped past what we hold, remember
    // how much future input to skip.
    const std::size_t nextStart = m_nextK * dec;
    const std::size_t received = m_baseIndex + m_hist.size();
    if (nextStart <= received) {
        m_hist.erase(m_hist.begin(),
                     m_hist.begin() + (nextStart - m_baseIndex));
        m_baseIndex = nextStart;
    } else {
        m_hist.clear();
        m_skip += nextStart - received;
        m_baseIndex = received;
    }
    return out;
}

std::vector<Complex> BandTrimmer::flushStream()
{
    if (m_taps.empty()) {
        resetStream();
        return {};
    }
    // Zero-pad so the final windows complete, then reset.
    const std::vector<Complex> pad(m_taps.size(), Complex{0.0f, 0.0f});
    std::vector<Complex> out = processStream(pad);
    resetStream();
    return out;
}

} // namespace duality
