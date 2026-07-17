#pragma once

#include "core/Types.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QPushButton;

namespace duality {

// One recording stage: tuning parameters, duration and trigger mode, with
// MONITOR (no file) / RECORD / STOP controls.
class CapturePanel : public QWidget {
    Q_OBJECT
public:
    explicit CapturePanel(QWidget *parent = nullptr);

    StreamParams streamParams() const;
    double durationSec() const;
    QString trigger() const;
    double peakThresholdDb() const;
    // ± capture range (half-width) in Hz, used for the spectrum overlay and to
    // trim auto-captured segments.
    double captureRangeHz() const;
    // How long an auto segment stays open after the signal drops below
    // threshold, bridging modulation gaps so one transmission is not split.
    double hangTimeMs() const;

    void setRunning(bool running);

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
    QDoubleSpinBox *m_bandwidth;
    QDoubleSpinBox *m_gain;
    QDoubleSpinBox *m_duration;
    QDoubleSpinBox *m_peakThreshold;
    QDoubleSpinBox *m_captureRange;
    QDoubleSpinBox *m_hangTime;
    QComboBox *m_trigger;
    QPushButton *m_monitorButton;
    QPushButton *m_recordButton;
    QPushButton *m_stopButton;
};

} // namespace duality
