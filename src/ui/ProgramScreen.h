#pragma once

#include <QWidget>

namespace duality {

// Wraps a program's content beneath a slim top bar (a back arrow and the
// program title) so the user can return to the home grid. Emits backRequested()
// when the arrow is tapped.
class ProgramScreen : public QWidget {
    Q_OBJECT
public:
    ProgramScreen(const QString &title, QWidget *content,
                  QWidget *parent = nullptr);

signals:
    void backRequested();
};

} // namespace duality
