#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class QEvent;
class QLabel;
class QWidget;

namespace duality {

// Shows small, self-dismissing "toast" messages stacked in the bottom-right
// corner of a host widget. Toasts are overlay children of the host, so they
// float above its normal content.
class ToastManager : public QObject {
    Q_OBJECT
public:
    explicit ToastManager(QWidget *host, QObject *parent = nullptr);

    void show(const QString &message, int durationMs = 3000);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reposition();

    QWidget *m_host;
    QList<QPointer<QLabel>> m_toasts;
};

} // namespace duality
