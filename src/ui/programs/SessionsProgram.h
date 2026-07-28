#pragma once

#include "storage/SessionStore.h"

#include <QWidget>

#include <memory>

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace duality {

class PlaybackPanel;
class Session;

// Full session manager: browse every session, drill into one to see its
// recordings, read a recording's metadata, replay it, and delete recordings or
// whole sessions. Opened from the home grid (at the list) or from a FOB/CAPTURE
// session tile (jumped straight to that session). Playback runs through the
// shell's shared pipeline via the embedded PlaybackPanel.
class SessionsProgram : public QWidget {
    Q_OBJECT
public:
    explicit SessionsProgram(SessionStore *store, QWidget *parent = nullptr);

    // The embedded player, wired to the playback pipeline by MainWindow.
    PlaybackPanel *playbackPanel() const { return m_player; }

    // Rebuild from the store (keeps the current page when possible).
    void refresh();
    // Show the top-level session list.
    void showList();
    // Jump straight to one session's detail page.
    void openSession(const QString &sessionDir);

signals:
    void newSessionRequested();
    // A session or recording was created/deleted; the shell should refresh the
    // other views (FOB/CAPTURE lists, playback dropdowns, debug).
    void sessionsChanged();

private:
    void rebuildSessionList();
    void rebuildDetail();
    void showRecording(int index);
    void promptDeleteSession();
    void promptDeleteAllSessions();
    void promptDeleteRecording(int index);

    SessionStore *m_store;
    QStackedWidget *m_stack;

    // List page.
    QListWidget *m_sessions;

    // Detail page.
    QLabel *m_detailTitle;
    QLabel *m_detailSubtitle;
    QListWidget *m_recordings;
    QWidget *m_recordingBox; // info + delete + player, hidden until a selection
    QLabel *m_info;
    QPushButton *m_deleteRecordingButton;
    PlaybackPanel *m_player;

    std::unique_ptr<Session> m_session; // session shown on the detail page
    int m_currentRecording = -1;
};

} // namespace duality
