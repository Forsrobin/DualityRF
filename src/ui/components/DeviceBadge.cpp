#include "ui/components/DeviceBadge.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>

namespace duality {

DeviceBadge::DeviceBadge(const QString &tag, const QString &color,
                         QWidget *parent)
    : QFrame(parent)
    , m_name(new QLabel(this))
{
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Select %1 device").arg(tag));

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(3);

    auto *tagLabel = new QLabel(tag, this);
    tagLabel->setAlignment(Qt::AlignCenter);
    tagLabel->setStyleSheet(
        QStringLiteral("background: %1; color: #000000; font-weight: bold;"
                       " padding: 1px 3px;")
            .arg(color));
    // Children shouldn't swallow clicks; the whole badge is one hit target.
    tagLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(tagLabel);

    m_name->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(m_name);

    setDeviceName(QString());
}

void DeviceBadge::setDeviceName(const QString &name)
{
    if (name.isEmpty()) {
        m_name->setText(QStringLiteral("—"));
        m_name->setStyleSheet(
            QStringLiteral("color: #808080; background: transparent;"));
        return;
    }
    const QString shown = name.size() <= kMaxNameChars
                              ? name
                              : name.left(kMaxNameChars - 1) +
                                    QStringLiteral("…");
    m_name->setText(shown);
    m_name->setStyleSheet(
        QStringLiteral("color: #ffffff; background: transparent;"));
}

void DeviceBadge::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QFrame::mousePressEvent(event);
}

} // namespace duality
