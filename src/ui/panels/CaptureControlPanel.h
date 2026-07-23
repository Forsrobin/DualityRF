#pragma once

#include "core/Types.h"
#include "ui/panels/ICaptureControls.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QPushButton;

namespace duality {

// Simplified recording controls for the standalone CAPTURE program: no monitor,
// no concurrent TX and no replay. A single MANUAL/AUTO mode selector reveals the
// auto-only detection settings, and one RECORD button toggles to a red STOP
// while a capture runs.
class CaptureControlPanel : public QWidget, public ICaptureControls {
    Q_OBJECT
public:
    explicit CaptureControlPanel(QWidget *parent = nullptr);

    StreamParams streamParams() const override;
    QString trigger() const override;
    double peakThresholdDb() const override;
    double captureRangeHz() const override;
    double hangTimeMs() const override;
    // No replay in this program.
    bool replayMode() const override { return false; }

    void setRunning(bool running) override;

signals:
    void recordRequested();
    void stopRequested();
    void paramsChanged();
    void peakThresholdChanged(double thresholdDb);

private:
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_bandwidth;
    QDoubleSpinBox *m_gain;
    QDoubleSpinBox *m_peakThreshold;
    QDoubleSpinBox *m_captureRange;
    QDoubleSpinBox *m_hangTime;
    QComboBox *m_mode;
    // Container holding the auto-only fields, hidden entirely in manual mode.
    QWidget *m_autoBox;
    QPushButton *m_recordButton;
    bool m_running = false;
};

} // namespace duality
