#pragma once

#include "storage/SessionStore.h"

#include <QWidget>

#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;

namespace duality {

// Replays a stored recording through the transmitter chosen in the device
// panel. The recording is chosen by the caller (the Sessions program's
// recording list); this panel just tunes and transmits it.
class PlaybackPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackPanel(SessionStore *store, QWidget *parent = nullptr);

    // Re-validate the current selection (e.g. after a delete elsewhere).
    void refresh();
    // Select the recording to play (from the session list).
    void select(const QString &sessionDir, const QString &file);
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
    SessionStore *m_store;
    std::unique_ptr<Session> m_session;

    QDoubleSpinBox *m_frequency;
    QDoubleSpinBox *m_sampleRate;
    QDoubleSpinBox *m_gain;
    QComboBox *m_speed;
    QCheckBox *m_repeat;
    QProgressBar *m_progress;
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QLabel *m_progressLabel;

    RecordingMetadata m_meta;
    QString m_path;
};

} // namespace duality
