#pragma once

#include <QPointer>
#include <QWidget>

class QLabel;
class QPushButton;
class QAbstractSpinBox;

namespace duality {

// On-screen numeric keypad for touch use. It installs an application-wide focus
// filter and, whenever a QSpinBox / QDoubleSpinBox anywhere in the app gains
// focus, appears at the bottom of its host window so the value can be tapped in.
// A header row shows the original value alongside the live edit, and every key
// commits into the field as it is pressed. All keys are Qt::NoFocus so tapping
// them never steals focus from the field being edited (the standard on-screen
// keyboard trick).
class NumpadOverlay : public QWidget {
    Q_OBJECT
public:
    // `host` is the window the pad overlays (its bottom strip). The pad parents
    // itself to `host` and starts hidden.
    explicit NumpadOverlay(QWidget *host);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    // Resolve the editable spin box behind a freshly focused object, or nullptr
    // if it is not a numeric field we drive (also unwraps a spin box's internal
    // line edit).
    QAbstractSpinBox *resolveTarget(QObject *obj) const;

    void attachTo(QAbstractSpinBox *target);
    void detach();

    void appendText(const QString &text); // a digit or decimal separator
    void backspace();
    void clearInput();
    void toggleSign();

    void commit();        // push the buffer into the target (clamped by it)
    void updateDisplay(); // refresh the current / new header labels
    void reposition();    // pin to the bottom strip of the host

    QPushButton *makeKey(const QString &label);

    QWidget *m_host;                     // window we overlay
    QPointer<QAbstractSpinBox> m_target; // field being edited, if any
    QString m_buffer;                    // edit text, '.' as the decimal point
    QString m_originalDisplay;           // field's shown text when editing began

    QLabel *m_currentLabel;
    QLabel *m_newLabel;
    QPushButton *m_dotKey;  // decimal keys, disabled for integer fields
    QPushButton *m_commaKey;
    QPushButton *m_signKey; // enabled only when the field allows negatives
};

} // namespace duality
