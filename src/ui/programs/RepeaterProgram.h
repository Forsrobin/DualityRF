#pragma once

#include "core/Types.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace duality {

struct SpectrumMarker;
class HazardButton;
class SpectrumWidget;
class WaterfallWidget;

// Standalone store-and-forward repeater: receive on a channel, and whenever a
// transmission is detected (a simple power/peak detector), buffer its raw IQ in
// memory and immediately retransmit it once — so a weak signal is picked up and
// rebroadcast to reach further. An offset (default 0) shifts the retransmit
// carrier, so the burst can be repeated on a different frequency. MainWindow
// runs the RepeaterPipeline and feeds the live spectrum back through
// pushSpectrum(); the program owns only the parameters and the visualization.
class RepeaterProgram : public QWidget {
    Q_OBJECT
public:
    explicit RepeaterProgram(QWidget *parent = nullptr);

    // Rich-text help shown by the top-bar info button.
    static QString infoText();

    // Receive tuning: carrier, sample rate (span) and RX gain.
    StreamParams rxParams() const;
    // Signed retransmit offset from the receive carrier, in Hz (0 = same freq).
    double offsetHz() const;
    double txGainDb() const;
    // Trigger level for the power detector, in dBFS.
    float thresholdDb() const;
    // Half-width (± Hz) the captured burst is band-limited to before it is
    // replayed. 0 keeps the full received span.
    double captureRangeHz() const;

    // Phase of the repeat cycle, shown in the status readout.
    enum class Phase { Idle, Listening, Capturing, Transmitting };
    void setPhase(Phase phase);
    void setRepeatCount(int count);

    // Reflect the running state: toggles the hazard button and locks the
    // parameter controls while armed.
    void setRunning(bool running);
    // Feed one live spectrum frame into the FFT trace and the waterfall.
    void pushSpectrum(const QVector<float> &row);
    // Draw the detected-peak marker lines on the FFT trace.
    void setPeakMarkers(const QVector<SpectrumMarker> &markers);

signals:
    void startRequested();
    void stopRequested();

private:
    void applyPreset(int index);
    void updateOverlay();
    void updateStatusLabel();

    QComboBox *m_preset;
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_rxGain;
    QSpinBox *m_threshold;
    QDoubleSpinBox *m_captureRange;
    QDoubleSpinBox *m_offset;
    QDoubleSpinBox *m_txGain;
    QLabel *m_status;
    QWidget *m_controls;
    HazardButton *m_startButton;
    SpectrumWidget *m_spectrum;
    WaterfallWidget *m_waterfall;

    Phase m_phase = Phase::Idle;
    int m_repeatCount = 0;
};

} // namespace duality
