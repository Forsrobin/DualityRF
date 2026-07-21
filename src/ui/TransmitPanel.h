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
    // Programmatically toggle the enable/disable button (emits enabledChanged
    // like a user click).
    void setTransmitEnabled(bool enabled);
    WaveformConfig waveformConfig() const;
    double txGainDb() const;

signals:
    // Emitted when the user toggles the enable/disable button so a running
    // capture can start or stop the transmission live.
    void enabledChanged(bool enabled);
    // Emitted when any waveform parameter changes so a running transmission can
    // swap to the new waveform without stopping.
    void waveformChanged();

private slots:
    void onTypeChanged();
    void browseFile();

private:
    QGroupBox *m_group;
    QPushButton *m_enableButton;
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
