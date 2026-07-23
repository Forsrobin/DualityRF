#pragma once

#include "core/Types.h"
#include "ui/panels/ICaptureControls.h"

#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;

namespace duality {

// Controls for the LOOKING glass: a monitor-only analysis tool. Just the basic
// RX tuning, a peak-detection toggle, and one START button that turns into a red
// STOP while running. No recording, no trigger, no concurrent TX.
class LookingPanel : public QWidget, public ICaptureControls {
    Q_OBJECT
public:
    explicit LookingPanel(QWidget *parent = nullptr);

    StreamParams streamParams() const override;
    QString trigger() const override { return QStringLiteral("manual"); }
    // Editable peak level, revealed only while peak detection is enabled.
    double peakThresholdDb() const override;
    double captureRangeHz() const override { return 0.0; }
    double hangTimeMs() const override { return 0.0; }
    bool replayMode() const override { return false; }
    bool peakDetectionEnabled() const override;

    void setRunning(bool running) override;

signals:
    void startRequested();
    void stopRequested();
    void paramsChanged();
    void peakDetectionToggled(bool enabled);
    void peakThresholdChanged(double thresholdDb);

private:
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_bandwidth;
    QDoubleSpinBox *m_gain;
    QCheckBox *m_peakDetection;
    QDoubleSpinBox *m_peakThreshold;
    QWidget *m_peakBox; // holds the peak-level row, shown only when enabled
    QPushButton *m_startButton;
    bool m_running = false;
};

} // namespace duality
