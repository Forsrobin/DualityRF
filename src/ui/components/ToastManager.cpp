#include "ui/components/ToastManager.h"

#include <QEvent>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

namespace duality {

ToastManager::ToastManager(QWidget *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
    if (m_host)
        m_host->installEventFilter(this);
}

void ToastManager::show(const QString &message, int durationMs)
{
    if (!m_host)
        return;

    auto *toast = new QLabel(message, m_host);
    toast->setObjectName(QStringLiteral("toast"));
    toast->setStyleSheet(QStringLiteral(
        "#toast { background:#000000; color:#ffffff; border:1px solid #ffffff; "
        "padding:6px 10px; }"));
    toast->setWordWrap(true);
    toast->setMaximumWidth(m_host->width() - 16);
    toast->adjustSize();
    toast->show();
    toast->raise();
    m_toasts.append(toast);
    reposition();

    // Context object is `this`, so a pending timer is cancelled if the manager
    // (and its host) go away first.
    QTimer::singleShot(durationMs, this, [this, toast] {
        m_toasts.removeAll(toast);
        if (toast)
            toast->deleteLater();
        reposition();
    });
}

bool ToastManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_host && event->type() == QEvent::Resize)
        reposition();
    return QObject::eventFilter(watched, event);
}

void ToastManager::reposition()
{
    if (!m_host)
        return;

    constexpr int margin = 8;
    constexpr int spacing = 6;

    // Keep clear of the status bar if the host has one.
    int bottom = m_host->height();
    if (auto *statusBar = m_host->findChild<QStatusBar *>())
        bottom -= statusBar->height();

    // Newest toast at the very bottom, older ones stacked above it.
    int y = bottom - margin;
    for (int i = m_toasts.size() - 1; i >= 0; --i) {
        QLabel *toast = m_toasts[i];
        if (!toast)
            continue;
        y -= toast->height();
        toast->move(m_host->width() - toast->width() - margin, y);
        toast->raise();
        y -= spacing;
    }
}

} // namespace duality
