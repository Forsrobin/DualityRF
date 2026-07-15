#pragma once

#include "storage/RecordingMetadata.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace duality {

// Replays the recording selected in the session browser through the
// transmitter chosen in the device panel.
class PlaybackPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackPanel(QWidget *parent = nullptr);

    void setRecording(const RecordingMetadata &meta, const QString &absPath);
    bool hasRecording() const { return !m_path.isEmpty(); }
    QString recordingPath() const { return m_path; }
    RecordingMetadata recordingMetadata() const { return m_meta; }

    StreamParams streamParams() const; // sampleRateHz = un-scaled rate
    double speed() const;
    bool repeat() const;

    void setPlaying(bool playing);
    void setProgress(double seconds, double totalSeconds);

signals:
    void playRequested();
    void stopRequested();

private:
    QLabel *m_recordingLabel;
    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_gain;
    QComboBox *m_speed;
    QCheckBox *m_repeat;
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QLabel *m_progressLabel;

    RecordingMetadata m_meta;
    QString m_path;
};

} // namespace duality
