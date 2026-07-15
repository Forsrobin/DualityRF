#include "dsp/Window.h"

#include <cmath>
#include <numbers>

namespace duality {

std::vector<float> makeWindow(WindowType type, std::size_t size)
{
    std::vector<float> w(size, 1.0f);
    const double n1 = static_cast<double>(size - 1);
    for (std::size_t i = 0; i < size; ++i) {
        const double x = 2.0 * std::numbers::pi * static_cast<double>(i) / n1;
        switch (type) {
        case WindowType::Rectangular:
            break;
        case WindowType::Hann:
            w[i] = static_cast<float>(0.5 - 0.5 * std::cos(x));
            break;
        case WindowType::Hamming:
            w[i] = static_cast<float>(0.54 - 0.46 * std::cos(x));
            break;
        case WindowType::Blackman:
            w[i] = static_cast<float>(0.42 - 0.5 * std::cos(x)
                                      + 0.08 * std::cos(2.0 * x));
            break;
        }
    }

    // Compensate coherent gain so windowing does not shift absolute levels.
    double sum = 0.0;
    for (float v : w)
        sum += v;
    const float norm = static_cast<float>(size / sum);
    for (float &v : w)
        v *= norm;
    return w;
}

const char *windowName(WindowType type)
{
    switch (type) {
    case WindowType::Rectangular: return "Rectangular";
    case WindowType::Hann: return "Hann";
    case WindowType::Hamming: return "Hamming";
    case WindowType::Blackman: return "Blackman";
    }
    return "?";
}

} // namespace duality
