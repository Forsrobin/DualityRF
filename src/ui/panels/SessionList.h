#pragma once

#include "storage/SessionStore.h"

#include <QWidget>

class QListWidget;

namespace duality {

// A flat, non-expandable list of sessions for the FOB and CAPTURE program tabs.
// Each row shows the session name, its recording count and age; the newest
// session is tagged. Tapping a row asks the shell to open the full SESSIONS
// management program on that session (this widget does no editing itself).
class SessionList : public QWidget {
    Q_OBJECT
public:
    explicit SessionList(SessionStore *store, QWidget *parent = nullptr);

    void refresh();

signals:
    void sessionActivated(const QString &sessionDir);

private:
    SessionStore *m_store;
    QListWidget *m_list;
};

} // namespace duality
