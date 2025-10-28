#include "WaveformWidget.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <algorithm>

WaveformWidget::WaveformWidget(QWidget *parent) : QWidget(parent) {
  setMinimumHeight(160);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void WaveformWidget::setData(const QVector<float> &samples, double durationSec,
                             const QVector<int> &peakIndices) {
  data = samples;
  peaks = peakIndices;
  durationSeconds = durationSec;
  update();
}

void WaveformWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  const QRect r = rect();
  p.fillRect(r, Qt::black);

  // Draw frame
  QPen framePen(QColor(0, 200, 200));
  framePen.setWidth(1);
  p.setPen(framePen);
  p.drawRect(r.adjusted(0, 0, -1, -1));

  if (data.isEmpty())
    return;

  // Plot area with small margins
  const int left = r.left() + 8;
  const int right = r.right() - 8;
  const int top = r.top() + 8;
  const int bottom = r.bottom() - 16;
  const QRect plot(left, top, right - left + 1, bottom - top + 1);

  // Draw baseline grid (simple)
  p.setPen(QColor(0, 80, 80));
  p.drawLine(plot.left(), bottom, plot.right(), bottom);         // zero line
  p.drawLine(plot.left(), top, plot.right(), top);               // max line
  p.drawLine(plot.left(), (top + bottom) / 2, plot.right(), (top + bottom) / 2); // mid

  // Render waveform as polyline
  QPen wavePen(QColor(0, 255, 255));
  wavePen.setWidth(2);
  p.setPen(wavePen);
  const int N = data.size();
  for (int i = 1; i < N; ++i) {
    int x0 = plot.left() + (i - 1) * plot.width() / (N - 1);
    int x1 = plot.left() + i * plot.width() / (N - 1);
    float y0v = std::clamp(data[i - 1], 0.0f, 1.0f);
    float y1v = std::clamp(data[i], 0.0f, 1.0f);
    int y0 = bottom - int(y0v * plot.height());
    int y1 = bottom - int(y1v * plot.height());
    p.drawLine(x0, y0, x1, y1);
  }

  // Draw peaks as subdued vertical markers
  if (!peaks.isEmpty()) {
    QPen peakPen(QColor(0, 255, 128, 90)); // semi-transparent
    peakPen.setWidth(1);
    peakPen.setStyle(Qt::DashLine);
    p.setPen(peakPen);
    // enforce a minimum pixel spacing to avoid visual clutter
    const int minDx = std::max(6, plot.width() / 150);
    int lastX = -10000;
    for (int idx : peaks) {
      if (idx < 0 || idx >= N)
        continue;
      int x = plot.left() + idx * plot.width() / (N - 1);
      if (x - lastX < minDx)
        continue;
      lastX = x;
      // shorter tick from 20% down to bottom for less dominance
      int tickTop = top + plot.height() / 5;
      p.drawLine(x, tickTop, x, bottom);
    }
  }

  // Time labels (start/end)
  p.setPen(QColor(0, 200, 200));
  QString t0 = QString("0.0 s");
  QString t1 = QString::number(durationSeconds, 'f', (durationSeconds < 10.0 ? 2 : 1)) + " s";
  p.drawText(plot.left(), r.bottom() - 2, t0);
  int tw = p.fontMetrics().horizontalAdvance(t1);
  p.drawText(plot.right() - tw, r.bottom() - 2, t1);
}
