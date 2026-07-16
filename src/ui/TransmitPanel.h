#pragma once

#include "dsp/WaveformGenerator.h"

#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLineEdit;
class QPushButton;

namespace duality {

// Concurrent-transmission configuration for a capture stage: waveform
// generator, its parameters and the TX gain. Frequency and sample rate
// follow the capture stage so both streams share the observed channel.
class TransmitPanel : public QWidget {
    Q_OBJECT
public:
    explicit TransmitPanel(QWidget *parent = nullptr);

    bool transmitEnabled() const;
    WaveformConfig waveformConfig() const;
    double txGainDb() const;

signals:
    // Emitted when the user ticks/unticks the CONCURRENT TX group so a running
    // capture can start or stop the transmission live.
    void enabledChanged(bool enabled);

private slots:
    void onTypeChanged();
    void browseFile();

private:
    QGroupBox *m_group;
    QComboBox *m_type;
    QDoubleSpinBox *m_amplitude;
    QPushButton *m_offsetSign;
    QDoubleSpinBox *m_offset;
    QDoubleSpinBox *m_range;
    QLineEdit *m_filePath;
    QPushButton *m_browseButton;
    QDoubleSpinBox *m_gain;
};

} // namespace duality
