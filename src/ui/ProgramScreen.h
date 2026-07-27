#pragma once

#include <QWidget>

namespace duality {

class DeviceBadge;
class ProgramInfo;

// Wraps a program's content beneath a slim top bar: a back arrow and the
// program title on the left, and the selected TX/RX device badges on the right.
// Emits backRequested() when the arrow is tapped, and tx/rxClicked() when the
// respective device badge is tapped (so the shell can open the device selector).
// An optional info button (setInfo) sits next to the title and opens a modal
// describing the program.
class ProgramScreen : public QWidget {
    Q_OBJECT
public:
    ProgramScreen(const QString &title, QWidget *content,
                  QWidget *parent = nullptr);

    // Update the device names shown in the top-bar badges (empty = none).
    void setTxName(const QString &name);
    void setRxName(const QString &name);

    // Attach help for this program: reveals the top-bar info button, which
    // opens a scrollable modal with `body` (rich text) headed by `title`.
    void setInfo(const QString &title, const QString &body);

signals:
    void backRequested();
    void txClicked();
    void rxClicked();

private:
    DeviceBadge *m_txBadge;
    DeviceBadge *m_rxBadge;
    ProgramInfo *m_info;
    QString m_title;
};

} // namespace duality
