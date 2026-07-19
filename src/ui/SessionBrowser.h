#pragma once

#include "storage/SessionStore.h"

#include <QWidget>

#include <memory>
#include <vector>

class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace duality {

// Tree of stored sessions and their recordings; selecting a recording arms
// the playback panel.
class SessionBrowser : public QWidget {
    Q_OBJECT
public:
    explicit SessionBrowser(SessionStore *store, QWidget *parent = nullptr);

    void refresh();

signals:
    void recordingSelected(const QString &sessionDir,
                           const duality::RecordingMetadata &meta);
    void newSessionRequested();

private slots:
    void onSelectionChanged();
    void onContextMenu(const QPoint &pos);

private:
    // Confirm-and-delete, shared by the context menu and the per-row buttons.
    void promptDeleteRecording(int sessionIdx, int recIdx);
    void promptDeleteSession(int sessionIdx);

    SessionStore *m_store;
    QTreeWidget *m_tree;
    QPushButton *m_newSessionButton;
    std::vector<std::unique_ptr<Session>> m_sessions;
};

} // namespace duality
