#pragma once

#include "core/Types.h"
#include "dsp/WaveformGenerator.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace duality {

class HazardButton;
class SpectrumWidget;
class WaterfallWidget;

// Standalone reactive-jammer program (a "follower" / triggered jammer): arm it
// on a band and it watches the receiver for a transmission in a chosen watch
// window; the instant the in-band peak crosses the threshold it fires a short
// jamming burst on the transmitter, then holds off for a cooldown before
// re-arming. Unlike the always-on JAM program this only radiates in response to
// a detected signal, so it demonstrates the classic RX -> decision -> TX loop.
// It needs both a receiver (to detect) and a transmitter (to jam). MainWindow
// runs the detection loop and feeds the live spectrum back through
// pushSpectrum(); the program owns only the parameters and the visualization.
class SentryProgram : public QWidget {
    Q_OBJECT
public:
    explicit SentryProgram(QWidget *parent = nullptr);

    // Rich-text help shown by the top-bar info button.
    static QString infoText();

    // Carrier, sample rate (span) and TX gain — shared by the RX monitor and
    // the jam burst, which tune to the same channel.
    StreamParams streamParams() const;
    // The waveform fired on each trigger.
    WaveformConfig waveformConfig() const;

    // Watch window: centre (carrier + offset) and full width, in Hz. A frame
    // whose peak power inside this window exceeds thresholdDb() triggers a burst.
    double watchCenterHz() const;
    double watchWidthHz() const;
    float thresholdDb() const;
    // Burst length and post-burst hold-off, in milliseconds.
    int burstMs() const;
    int cooldownMs() const;

    // Reflect the running state: toggles the hazard button and locks the
    // parameter controls while armed.
    void setRunning(bool running);
    // Update the "triggers fired" readout.
    void setTriggerCount(int count);
    // Highlight the state (Listening / Firing) in the readout so the operator
    // sees when a burst is live.
    void setFiring(bool firing);
    // Feed one live spectrum frame into the FFT trace and the waterfall.
    void pushSpectrum(const QVector<float> &row);

signals:
    void startRequested(); // ARM
    void stopRequested();  // DISARM

private:
    // Fill the frequency/rate fields from the chosen preset (no-op for Custom).
    void applyPreset(int index);
    // Enable the file row only for the IQ-file jam waveform.
    void onTypeChanged();
    void browseFile();
    // Redraw the watch-window overlay from the current field values.
    void updateOverlay();
    void updateStatusLabel();

    QComboBox *m_preset;
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_watchOffset;
    QDoubleSpinBox *m_watchWidth;
    QSpinBox *m_threshold;
    QComboBox *m_type;
    QDoubleSpinBox *m_gain;
    QLineEdit *m_filePath;
    QPushButton *m_browseButton;
    QSpinBox *m_burst;
    QSpinBox *m_cooldown;
    QLabel *m_status;
    QWidget *m_controls;
    HazardButton *m_startButton;
    SpectrumWidget *m_spectrum;
    WaterfallWidget *m_waterfall;

    int m_triggerCount = 0;
    bool m_firing = false;
    bool m_armed = false;
};

} // namespace duality
