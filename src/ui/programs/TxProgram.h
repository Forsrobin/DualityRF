#pragma once

#include "core/Types.h"
#include "dsp/WaveformGenerator.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;

namespace duality {

class HazardButton;
class SpectrumWidget;
class WaterfallWidget;

// Standalone transmitter program: pick a frequency (preset or manual), a
// waveform (white/gaussian noise, CW, sine, IQ file) and its offset/bandwidth,
// set the TX gain, then START. The FFT always shows the affected band as an
// overlay (like the capture visualization) with the waterfall beneath it; both
// stay visible. Transmission is driven by MainWindow, which feeds the live
// spectrum back through pushSpectrum().
class TxProgram : public QWidget {
    Q_OBJECT
public:
    explicit TxProgram(QWidget *parent = nullptr);

    // Carrier, sample rate (span) and TX gain for the transmitter.
    StreamParams streamParams() const;
    // The selected waveform with its offset and bandwidth.
    WaveformConfig waveformConfig() const;

    // Reflect the running state: toggles the hazard button and locks the
    // parameter controls while transmitting.
    void setRunning(bool running);
    // Feed one live spectrum frame into the FFT trace and the waterfall.
    void pushSpectrum(const QVector<float> &row);

signals:
    void startRequested();
    void stopRequested();

private:
    // Fill the frequency/rate fields from the chosen preset (no-op for Custom).
    void applyPreset(int index);
    // Enable the file row only for the IQ-file waveform.
    void onTypeChanged();
    void browseFile();
    // Redraw the affected-band overlay from the current field values.
    void updateOverlay();
    // Centre (carrier + offset) and full width of the affected band, in Hz.
    double affectedCenterHz() const;
    double affectedWidthHz() const;

    QComboBox *m_preset;
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_gain;
    QComboBox *m_type;
    QDoubleSpinBox *m_offset;
    QDoubleSpinBox *m_bandwidth;
    QLineEdit *m_filePath;
    QPushButton *m_browseButton;
    QWidget *m_controls;
    HazardButton *m_startButton;
    SpectrumWidget *m_spectrum;
    WaterfallWidget *m_waterfall;
};

} // namespace duality
