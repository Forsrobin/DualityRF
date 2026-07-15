#pragma once

#include <QRgb>

class QApplication;

namespace duality {

// Application-wide monochrome style: black background, white foreground,
// square corners, touch-friendly hit targets. The only color in the app is
// produced by spectrumColor() inside the plot widgets.
namespace Theme {

void apply(QApplication &app);

// Intensity colormap for spectrum visualizations; t in [0,1].
QRgb spectrumColor(float t);

// Shared dB display range for all plots.
inline constexpr float kMinDb = -110.0f;
inline constexpr float kMaxDb = 0.0f;

inline float normalizeDb(float db)
{
    return (db - kMinDb) / (kMaxDb - kMinDb);
}

} // namespace Theme

} // namespace duality
