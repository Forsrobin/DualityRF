#pragma once

#include "core/Types.h"
#include "ui/panels/ICaptureControls.h"

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace duality {

// One recording stage: tuning parameters, duration and trigger mode, with
// MONITOR (no file) / RECORD / STOP controls.
class CapturePanel : public QWidget, public ICaptureControls {
    Q_OBJECT
public:
    explicit CapturePanel(QWidget *parent = nullptr);

    StreamParams streamParams() const override;
    // The FOB only runs the auto trigger, so this always returns "auto".
    QString trigger() const override;
    double peakThresholdDb() const override;
    // ± capture range (half-width) in Hz, used for the spectrum overlay and to
    // trim auto-captured segments.
    double captureRangeHz() const override;
    // How long an auto segment stays open after the signal drops below
    // threshold, bridging modulation gaps so one transmission is not split.
    double hangTimeMs() const override;
    // Replay mode: capture two auto segments, then (optionally) drop concurrent
    // TX, switch to monitor and replay the first. When off, keep capturing
    // segments until the user stops.
    bool replayMode() const override;
    // Whether the concurrent TX is dropped after replay mode's second capture.
    // On by default; unchecking leaves the TX running through the replay.
    bool disableTxAfterCapture() const override;

    void setRunning(bool running) override;
    // Reflects the CONCURRENT TX enable state (owned by the transmit panel) in
    // a small indicator here, so it is visible without switching tabs.
    void setConcurrentTxEnabled(bool enabled);

signals:
    void monitorRequested();
    void recordRequested();
    void stopRequested();
    // Frequency, sample rate or bandwidth edited (drives the capture-range
    // overlay on the spectrum).
    void paramsChanged();
    void peakThresholdChanged(double thresholdDb);

private:
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_gain;
    QDoubleSpinBox *m_peakThreshold;
    QDoubleSpinBox *m_captureRange;
    QDoubleSpinBox *m_hangTime;
    QCheckBox *m_replayMode;
    QCheckBox *m_disableTxAfterCapture;
    QLabel *m_txIndicator;
    QPushButton *m_monitorButton;
    QPushButton *m_recordButton;
    QPushButton *m_stopButton;
};

} // namespace duality
