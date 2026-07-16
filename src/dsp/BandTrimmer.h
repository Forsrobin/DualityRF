#pragma once

#include "core/Types.h"

#include <span>
#include <vector>

namespace duality {

// Band-limits a baseband IQ capture to a narrow band of interest centered on
// DC (the receiver's tune frequency): a low-pass to ±halfWidthHz removes
// everything outside the configured capture range. The sample rate is left
// unchanged so the trimmed file stays replayable by the transmit hardware.
// Kept independent of the pipeline so the capture range can be changed without
// touching the capture logic.
class BandTrimmer {
public:
    // inputRateHz: sample rate of the captured stream.
    // halfWidthHz: the ± capture range (half of the retained bandwidth).
    BandTrimmer(double inputRateHz, double halfWidthHz);

    int decimation() const { return m_decimation; }
    double outputRateHz() const { return m_inputRateHz / m_decimation; }
    // The full bandwidth retained: 2 * halfWidthHz.
    double bandwidthHz() const { return m_bandwidthHz; }
    // True when the requested band is (near) the full input span, so process()
    // returns the samples unchanged.
    bool passthrough() const { return m_taps.empty(); }

    // One-shot: trim a whole buffer at once (used for short bursts).
    std::vector<Complex> process(std::span<const Complex> in) const;

    // Streaming: trim an arbitrarily long capture chunk-by-chunk while keeping
    // filter/decimation continuity across calls. resetStream() before the
    // first chunk; flushStream() after the last to emit the tail.
    void resetStream();
    std::vector<Complex> processStream(std::span<const Complex> in);
    std::vector<Complex> flushStream();

private:
    double m_inputRateHz;
    double m_bandwidthHz;
    int m_decimation = 1;
    std::vector<float> m_taps; // real windowed-sinc low-pass; empty = passthrough

    // Streaming state (see processStream()).
    std::vector<Complex> m_hist; // unconsumed input samples
    std::size_t m_baseIndex = 0; // absolute index of m_hist[0]
    std::size_t m_nextK = 0;     // next output index to produce
    std::size_t m_skip = 0;      // future input samples to discard (decim gap)
};

} // namespace duality
