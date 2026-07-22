#pragma once

#include <QWidget>

namespace duality {

class DeviceBadge;

// Wraps a program's content beneath a slim top bar: a back arrow and the
// program title on the left, and the selected TX/RX device badges on the right.
// Emits backRequested() when the arrow is tapped, and tx/rxClicked() when the
// respective device badge is tapped (so the shell can open the device selector).
class ProgramScreen : public QWidget {
    Q_OBJECT
public:
    ProgramScreen(const QString &title, QWidget *content,
                  QWidget *parent = nullptr);

    // Update the device names shown in the top-bar badges (empty = none).
    void setTxName(const QString &name);
    void setRxName(const QString &name);

signals:
    void backRequested();
    void txClicked();
    void rxClicked();

private:
    DeviceBadge *m_txBadge;
    DeviceBadge *m_rxBadge;
};

} // namespace duality
