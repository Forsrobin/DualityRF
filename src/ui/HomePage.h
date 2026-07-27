#pragma once

#include <QWidget>

class QGridLayout;
class QLabel;
class QToolButton;

namespace duality {

class DeviceBadge;

// The launcher screen: a top bar with the logo and "DualityRF" wordmark plus
// the selected TX/RX device badges (like the program screens, but with no back
// button), a 3-column grid of program tiles (icon over label) below it, and a
// slim footer showing the app version (left) and the wall-clock time (right).
// Register programs with addProgram(); each tile emits programActivated()
// with its registration index.
class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(QWidget *parent = nullptr);

    // Append a program tile. The index passed to programActivated() matches the
    // order of these calls (0, 1, 2, ...).
    void addProgram(const QString &name, const QString &iconPath);
    void addExitTile();

    // Update the device names shown in the top-bar badges (empty = none).
    void setTxName(const QString &name);
    void setRxName(const QString &name);

signals:
    void programActivated(int index);
    void exitRequested();
    // Emitted when the respective device badge is tapped, so the shell can open
    // the device selector.
    void txClicked();
    void rxClicked();

private:
    // Builds a grid tile with the shared geometry so program tiles and the EXIT
    // tile are laid out identically.
    QToolButton *createTile(const QString &text);
    // Adds a tile to the next grid slot and pins that row to the tile height.
    void placeTile(QToolButton *tile);

    QGridLayout *m_grid;
    QLabel *m_clock;
    DeviceBadge *m_txBadge;
    DeviceBadge *m_rxBadge;
    int m_count = 0;

    static constexpr int kColumns = 3;
    static constexpr int kTileHeight = 64;
    static constexpr int kHeaderHeight = 64;
    static constexpr int kFooterHeight = 32;
};

} // namespace duality
