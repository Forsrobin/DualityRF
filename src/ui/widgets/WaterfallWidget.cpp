#include "ui/widgets/WaterfallWidget.h"

#include "ui/Theme.h"

#include <QPainter>

namespace duality {

namespace {
// QImage::flipped() is Qt 6.9+; fall back to the older mirrored() on the Qt 6.4
// shipped by Ubuntu (used in CI).
QImage flippedVertical(const QImage &image)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return image.flipped(Qt::Vertical);
#else
    return image.mirrored(false, true);
#endif
}
} // namespace

WaterfallWidget::WaterfallWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
}

void WaterfallWidget::ensureImage(int bins)
{
    if (m_image.width() == bins && !m_image.isNull())
        return;
    m_image = QImage(bins, kHistory, QImage::Format_RGB32);
    m_image.fill(Qt::black);
    m_nextRow = 0;
    m_filledRows = 0;
}

void WaterfallWidget::writeRow(int y, const QVector<float> &db)
{
    QRgb *line = reinterpret_cast<QRgb *>(m_image.scanLine(y));
    for (int x = 0; x < db.size(); ++x)
        line[x] = Theme::spectrumColor(Theme::normalizeDb(db[x]));
}

void WaterfallWidget::addRow(const QVector<float> &db)
{
    if (db.isEmpty())
        return;
    ensureImage(db.size());
    writeRow(m_nextRow, db);
    m_nextRow = (m_nextRow + 1) % kHistory;
    m_filledRows = std::min(m_filledRows + 1, kHistory);
    update();
}

void WaterfallWidget::setRows(const QVector<QVector<float>> &rows)
{
    clear();
    for (const auto &row : rows)
        if (!row.isEmpty()) {
            ensureImage(row.size());
            writeRow(m_nextRow, row);
            m_nextRow = (m_nextRow + 1) % kHistory;
            m_filledRows = std::min(m_filledRows + 1, kHistory);
        }
    update();
}

void WaterfallWidget::clear()
{
    m_image = QImage();
    m_nextRow = 0;
    m_filledRows = 0;
    update();
}

void WaterfallWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (m_image.isNull() || m_filledRows == 0)
        return;

    // Newest row at the top: unroll the circular buffer into two slices.
    const int newest = (m_nextRow - 1 + kHistory) % kHistory;
    const int rowsToShow = m_filledRows;
    const double rowHeight = static_cast<double>(height()) / rowsToShow;

    // Slice 1: rows [start..newest] where start is the oldest of the newest
    // contiguous run ending at `newest`.
    const int run1 = newest + 1; // rows 0..newest, newest first when flipped
    const QImage top =
        flippedVertical(m_image.copy(0, 0, m_image.width(), run1));
    painter.drawImage(QRectF(0, 0, width(),
                             rowHeight * std::min(run1, rowsToShow)),
                      top);
    if (rowsToShow > run1) {
        const int run2 = rowsToShow - run1;
        const QImage bottom = flippedVertical(
            m_image.copy(0, kHistory - run2, m_image.width(), run2));
        painter.drawImage(
            QRectF(0, rowHeight * run1, width(), rowHeight * run2), bottom);
    }
}

} // namespace duality
