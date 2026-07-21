#pragma once

#include <QWidget>

class QGridLayout;
class QLabel;

namespace duality {

// The launcher screen: a 3-column grid of program tiles (icon over label)
// filling the window, with a slim footer showing the app version (left) and
// the wall-clock time (right). Register programs with addProgram(); each tile
// emits programActivated() with its registration index.
class HomePage : public QWidget {
    Q_OBJECT
public:
    explicit HomePage(QWidget *parent = nullptr);

    // Append a program tile. The index passed to programActivated() matches the
    // order of these calls (0, 1, 2, ...).
    void addProgram(const QString &name, const QString &iconPath);

signals:
    void programActivated(int index);

private:
    QGridLayout *m_grid;
    QLabel *m_clock;
    int m_count = 0;

    static constexpr int kColumns = 3;
    static constexpr int kTileHeight = 64;
    static constexpr int kFooterHeight = 32;
};

} // namespace duality
