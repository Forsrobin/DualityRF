#include "ui/widgets/SpectrumWidget.h"

#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace duality {

namespace {

QString formatHz(double hz)
{
    if (std::abs(hz) >= 1e9)
        return QStringLiteral("%1 GHz").arg(hz / 1e9, 0, 'f', 3);
    if (std::abs(hz) >= 1e6)
        return QStringLiteral("%1 MHz").arg(hz / 1e6, 0, 'f', 3);
    if (std::abs(hz) >= 1e3)
        return QStringLiteral("%1 kHz").arg(hz / 1e3, 0, 'f', 1);
    return QStringLiteral("%1 Hz").arg(hz, 0, 'f', 0);
}

} // namespace

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setMouseTracking(false);
}

void SpectrumWidget::setAxis(double centerHz, double spanHz)
{
    m_centerHz = centerHz;
    m_spanHz = spanHz;
    update();
}

void SpectrumWidget::setTrace(const QVector<float> &db)
{
    m_trace = db;
    if (m_peakHold) {
        if (m_peaks.size() != db.size())
            m_peaks = db;
        else
            for (int i = 0; i < db.size(); ++i)
                m_peaks[i] = std::max(m_peaks[i], db[i]);
    }
    update();
}

void SpectrumWidget::setSecondaryTrace(const QVector<float> &db)
{
    m_secondary = db;
    update();
}

void SpectrumWidget::setPeakHold(bool enabled)
{
    m_peakHold = enabled;
    m_peaks.clear();
    update();
}

void SpectrumWidget::setMarkers(const QVector<SpectrumMarker> &markers)
{
    m_markers = markers;
    update();
}

void SpectrumWidget::setCaptureRange(double centerHz, double widthHz)
{
    m_rangeCenterHz = centerHz;
    m_rangeWidthHz = widthHz;
    update();
}

void SpectrumWidget::clear()
{
    m_trace.clear();
    m_secondary.clear();
    m_peaks.clear();
    m_markers.clear();
    update();
}

void SpectrumWidget::clampView()
{
    m_zoom = std::clamp(m_zoom, 1.0, 64.0);
    const double half = 0.5 / m_zoom;
    m_viewCenter = std::clamp(m_viewCenter, half, 1.0 - half);
}

int SpectrumWidget::frequencyToX(double hz) const
{
    if (m_spanHz <= 0.0)
        return -1;
    const double frac = 0.5 + (hz - m_centerHz) / m_spanHz;
    const double x = (frac - visibleStart()) / visibleWidth() * (width() - 1);
    return static_cast<int>(std::lround(x));
}

void SpectrumWidget::drawCaptureRange(QPainter &painter) const
{
    if (m_rangeWidthHz <= 0.0 || m_spanHz <= 0.0)
        return;
    const int w = width();
    const int h = height();
    const int lo = frequencyToX(m_rangeCenterHz - m_rangeWidthHz / 2.0);
    const int hi = frequencyToX(m_rangeCenterHz + m_rangeWidthHz / 2.0);
    if (hi < 0 || lo >= w)
        return;

    painter.fillRect(QRect(QPoint(std::max(lo, 0), 0),
                           QPoint(std::min(hi, w - 1), h - 1)),
                     QColor(0xff, 0xff, 0xff, 22));
    painter.setPen(QPen(QColor(0xd0, 0xd0, 0xd0), 1.0, Qt::DashLine));
    if (lo >= 0)
        painter.drawLine(lo, 0, lo, h - 1);
    if (hi < w)
        painter.drawLine(hi, 0, hi, h - 1);
}

void SpectrumWidget::drawMarkers(QPainter &painter) const
{
    if (m_markers.isEmpty() || m_spanHz <= 0.0)
        return;
    const int w = width();
    const int h = height();
    const QColor color(0x00, 0xe0, 0xe0);
    const int fontH = painter.fontMetrics().height();

    for (const SpectrumMarker &m : m_markers) {
        const int x = frequencyToX(m.frequencyHz);
        if (x < 0 || x >= w)
            continue;
        painter.setPen(QPen(color, 1.0));
        painter.drawLine(x, fontH + 4, x, h - 1);
        const QString label = formatHz(m.frequencyHz);
        const int tw = painter.fontMetrics().horizontalAdvance(label);
        painter.drawText(std::clamp(x - tw / 2, 2, w - tw - 2), fontH,
                         label);
    }
}

void SpectrumWidget::drawTrace(QPainter &painter, const QVector<float> &db,
                               const QColor &color) const
{
    if (db.isEmpty())
        return;
    const int w = width();
    const int h = height();
    const double start = visibleStart();
    const double vw = visibleWidth();

    QPainterPath path;
    for (int x = 0; x < w; ++x) {
        const double frac = start + vw * x / (w - 1);
        const int bin =
            std::clamp(static_cast<int>(frac * (db.size() - 1)), 0,
                       static_cast<int>(db.size()) - 1);
        const float t = Theme::normalizeDb(db[bin]);
        const double y = h - 1 - std::clamp(t, 0.0f, 1.0f) * (h - 1);
        if (x == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }
    painter.setPen(QPen(color, 1.0));
    painter.drawPath(path);
}

void SpectrumWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    const int w = width();
    const int h = height();

    // Grid: 10 dB horizontal lines, 5 frequency ticks.
    painter.setPen(QColor(0x30, 0x30, 0x30));
    const float dbRange = Theme::kMaxDb - Theme::kMinDb;
    for (float db = Theme::kMaxDb; db >= Theme::kMinDb; db -= 10.0f) {
        const int y = static_cast<int>((Theme::kMaxDb - db) / dbRange * (h - 1));
        painter.drawLine(0, y, w, y);
    }
    for (int i = 1; i < 5; ++i)
        painter.drawLine(w * i / 5, 0, w * i / 5, h);

    drawCaptureRange(painter);
    drawTrace(painter, m_secondary, QColor(0x80, 0x80, 0x80));
    drawTrace(painter, m_trace, Qt::white);
    if (m_peakHold)
        drawTrace(painter, m_peaks, QColor(0xb0, 0xb0, 0xb0));
    drawMarkers(painter);

    // Labels drawn last so they stay readable.
    painter.setPen(QColor(0xa0, 0xa0, 0xa0));
    for (float db = Theme::kMaxDb; db >= Theme::kMinDb; db -= 20.0f) {
        const int y = static_cast<int>((Theme::kMaxDb - db) / dbRange * (h - 1));
        painter.drawText(4, y + 12, QStringLiteral("%1 dB").arg(db));
    }
    if (m_spanHz > 0.0) {
        const double start = visibleStart();
        const double vw = visibleWidth();
        for (int i = 0; i <= 5; ++i) {
            const double frac = start + vw * i / 5.0;
            const double freq = m_centerHz + (frac - 0.5) * m_spanHz;
            const int x = w * i / 5;
            const QString label = formatHz(freq);
            const int tw = painter.fontMetrics().horizontalAdvance(label);
            painter.drawText(std::clamp(x - tw / 2, 2, w - tw - 2), h - 6,
                             label);
        }
    }
}

void SpectrumWidget::wheelEvent(QWheelEvent *event)
{
    const double factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
    const double mouseFrac =
        visibleStart() + visibleWidth() * event->position().x() /
                             std::max(1, width() - 1);
    m_zoom *= factor;
    m_zoom = std::clamp(m_zoom, 1.0, 64.0);
    // Keep the frequency under the cursor stationary while zooming.
    const double relX = event->position().x() / std::max(1, width() - 1);
    m_viewCenter = mouseFrac - (relX - 0.5) / m_zoom;
    clampView();
    update();
}

void SpectrumWidget::mousePressEvent(QMouseEvent *event)
{
    m_dragX = event->pos().x();
}

void SpectrumWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragX < 0)
        return;
    const int dx = event->pos().x() - m_dragX;
    m_dragX = event->pos().x();
    m_viewCenter -= visibleWidth() * dx / std::max(1, width() - 1);
    clampView();
    update();
}

void SpectrumWidget::mouseDoubleClickEvent(QMouseEvent *)
{
    m_zoom = 1.0;
    m_viewCenter = 0.5;
    m_peaks.clear();
    update();
}

} // namespace duality
