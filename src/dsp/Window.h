#pragma once

#include <vector>

namespace duality {

enum class WindowType {
    Rectangular,
    Hann,
    Hamming,
    Blackman,
};

// Coefficients scaled so a full-scale CW input reads ~0 dBFS after the FFT
// normalization in SpectrumAccumulator/SpectrumProcessor.
std::vector<float> makeWindow(WindowType type, std::size_t size);

const char *windowName(WindowType type);

} // namespace duality
