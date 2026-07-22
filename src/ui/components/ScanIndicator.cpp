#include "ui/components/ScanIndicator.h"

#include <QPainter>
#include <QTimer>

#include <cstdlib>

namespace duality {

ScanIndicator::ScanIndicator(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    setMinimumHeight(16);
    m_timer->setInterval(55);
    connect(m_timer, &QTimer::timeout, this, [this] {
        ++m_frame;
        update();
    });
}

void ScanIndicator::setScanning(bool on)
{
    if (m_scanning == on)
        return;
    m_scanning = on;
    if (on) {
        m_frame = 0;
        m_timer->start();
    } else {
        m_timer->stop();
    }
    update();
}

QSize ScanIndicator::sizeHint() const
{
    return {220, 16};
}

void ScanIndicator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    constexpr int gap = 2;
    const qreal segW =
        (width() - (kSegments - 1) * gap) / static_cast<qreal>(kSegments);

    // Bounce a lit position across the segments (0 → max → 0 …).
    const int period = 2 * (kSegments - 1);
    const int phase = period > 0 ? m_frame % period : 0;
    const int pos = phase < kSegments ? phase : period - phase;

    for (int i = 0; i < kSegments; ++i) {
        int value;
        if (m_scanning) {
            // Bright at the head, fading over a few trailing segments.
            const int dist = std::abs(i - pos);
            value = std::max(30, 255 - dist * 70);
        } else {
            value = 45; // idle: a dim, even row
        }
        painter.fillRect(QRectF(i * (segW + gap), 0, segW, height()),
                         QColor(value, value, value));
    }
}

} // namespace duality
