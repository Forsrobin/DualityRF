#include "ui/HazardButton.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>

namespace duality {

namespace {
const QColor kYellow(0xf5, 0xc4, 0x00);
constexpr int kStripe = 9; // yellow stripe width, px
constexpr int kGap = 9;    // black gap width, px
} // namespace

HazardButton::HazardButton(QWidget *parent)
    : QPushButton(parent)
{
    setCheckable(true);
}

void HazardButton::paintEvent(QPaintEvent *event)
{
    // Unchecked: leave the normal themed appearance untouched.
    if (!isChecked()) {
        QPushButton::paintEvent(event);
        return;
    }

    QPainter p(this);
    const QRect r = rect();
    p.setClipRect(r);
    p.fillRect(r, Qt::black);

    // Diagonal (45°) yellow stripes: slanted parallelograms marching across.
    p.setPen(Qt::NoPen);
    p.setBrush(kYellow);
    const int period = kStripe + kGap;
    const int h = r.height();
    for (int x = -h; x < r.width(); x += period) {
        QPolygon stripe;
        stripe << QPoint(x, h) << QPoint(x + kStripe, h)
               << QPoint(x + kStripe + h, 0) << QPoint(x + h, 0);
        p.drawPolygon(stripe);
    }

    // Label in a white box with black text, so it stays legible over the
    // stripes.
    const QString label = text();
    const QFontMetrics fm(font());
    QRect box(0, 0, fm.horizontalAdvance(label) + 12, fm.height() + 4);
    box.moveCenter(r.center());
    p.fillRect(box, Qt::white);
    p.setPen(Qt::black);
    p.drawText(box, Qt::AlignCenter, label);

    // 1px white frame to match the themed buttons.
    p.setPen(Qt::white);
    p.setBrush(Qt::NoBrush);
    p.drawRect(r.adjusted(0, 0, -1, -1));
}

} // namespace duality
