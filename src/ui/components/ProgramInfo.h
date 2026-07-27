#pragma once

#include <QString>
#include <QToolButton>

namespace duality {

// A top-bar "about this program" affordance: shows the info icon and, when
// tapped, opens a modal that explains what the program is and how to use it.
// The dialog scrolls when the text overflows the small popout. Lives in a
// program's top bar and stays hidden until setInfo() gives it content, so
// programs without help simply show nothing.
class ProgramInfo : public QToolButton {
    Q_OBJECT
public:
    explicit ProgramInfo(QWidget *parent = nullptr);

    // Heading and body (rich text / basic HTML) for the modal. An empty body
    // hides the button.
    void setInfo(const QString &title, const QString &body);

private:
    void openDialog();

    QString m_title;
    QString m_body;
};

} // namespace duality
