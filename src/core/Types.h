#pragma once

#include <complex>
#include <cstdint>

namespace duality {

using Complex = std::complex<float>;

enum class SampleFormat : std::uint8_t {
    Cs16, // interleaved int16 I,Q — native recording format
    Cf32, // interleaved float32 I,Q
};

constexpr std::size_t bytesPerSample(SampleFormat f)
{
    return f == SampleFormat::Cs16 ? 2 * sizeof(std::int16_t)
                                   : 2 * sizeof(float);
}

struct Range {
    double min = 0.0;
    double max = 0.0;

    bool contains(double v) const { return v >= min && v <= max; }
};

// The complete tuning vocabulary shared by receivers and transmitters.
struct StreamParams {
    double frequencyHz = 100e6;
    double sampleRateHz = 1e6;
    double bandwidthHz = 0.0; // 0 = let the driver choose
    double gainDb = 0.0;
};

} // namespace duality
