#pragma once

#include "core/Types.h"
#include "dsp/WaveformGenerator.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace duality {

class HazardButton;
class SpectrumWidget;
class WaterfallWidget;

// Standalone CW Morse beacon transmitter: type a message (a callsign/CQ ID),
// pick a frequency (preset or manual), a CW tone pitch and a keying speed, then
// START. The message is keyed as International Morse and looped, so it works as
// a station-ID beacon, a fox-hunt/ARDF transmitter or an antenna-test source.
// A live preview shows the dits/dahs before transmitting. The FFT/waterfall
// show the affected band, driven by MainWindow through pushSpectrum() when a
// receiver is selected to monitor.
class BeaconProgram : public QWidget {
    Q_OBJECT
public:
    explicit BeaconProgram(QWidget *parent = nullptr);

    // Rich-text help shown by the top-bar info button: what the program is and
    // how to use it.
    static QString infoText();

    // Carrier, sample rate (span) and TX gain for the transmitter.
    StreamParams streamParams() const;
    // The Morse waveform: message, tone offset (pitch) and keying speed.
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
    // Redraw the tone overlay and refresh the Morse preview from the fields.
    void updateOverlay();
    // Centre (carrier + tone) and estimated occupied width of the CW signal.
    double toneCenterHz() const;
    double occupiedWidthHz() const;

    QComboBox *m_preset;
    QLineEdit *m_message;
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_tone;
    QSpinBox *m_wpm;
    QDoubleSpinBox *m_gain;
    QLabel *m_preview;
    QWidget *m_controls;
    HazardButton *m_startButton;
    SpectrumWidget *m_spectrum;
    WaterfallWidget *m_waterfall;
};

} // namespace duality
