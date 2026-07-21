#pragma once

#include "storage/SessionStore.h"

#include <QWidget>

#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace duality {

// Replays a stored recording through the transmitter chosen in the device
// panel. Session and recording are picked from its own dropdowns (backed by
// the session store); external callers can pre-select a specific recording.
class PlaybackPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackPanel(SessionStore *store, QWidget *parent = nullptr);

    // Reload the session dropdown (call when sessions/recordings change).
    void refresh();
    // Pre-select a specific recording (e.g. from the browser or auto-replay).
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
    // Repopulate the recording dropdown from the selected session.
    void onSessionChanged(int index);
    // Load the recording at `index` (or clear when out of range) into the
    // fields and arm it for playback.
    void applyCurrent(int index);

    SessionStore *m_store;
    std::unique_ptr<Session> m_session;

    QComboBox *m_sessionCombo;
    QComboBox *m_recordingCombo;
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
