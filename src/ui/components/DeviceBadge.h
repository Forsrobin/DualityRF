#pragma once

#include <QFrame>

class QLabel;

namespace duality {

// A compact, clickable device indicator for the program top bar: a colored tag
// ("TX" red, "RX" blue) followed by a trimmed device name (or "—" when none is
// selected). Tapping anywhere on it emits clicked().
class DeviceBadge : public QFrame {
    Q_OBJECT
public:
    // `color` is any CSS color for the tag background (e.g. "#c0392b").
    DeviceBadge(const QString &tag, const QString &color,
                QWidget *parent = nullptr);

    // Empty name renders as a dim placeholder.
    void setDeviceName(const QString &name);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QLabel *m_name;

    static constexpr int kMaxNameChars = 8;
};

} // namespace duality
