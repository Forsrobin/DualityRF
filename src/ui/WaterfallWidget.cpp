
#include "WaterfallWidget.h"
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QString>
#include <algorithm>
#include <cmath>

static inline uchar mapCyan(float v) {
  v = std::clamp(v, 0.0f, 1.0f); // 0..1
  int c = int(v * 255.0f + 0.5f);
  return uchar(std::clamp(c, 0, 255));
}

WaterfallWidget::WaterfallWidget(QWidget *parent)
    : QWidget(parent), maxRows(600), nextRow(0), filled(false), dBmin(-60.0f),
      dBmax(0.0f), centerFrequencyHz(0.0), sampleRateHz(0.0) {
  setMinimumHeight(200);
  setStyleSheet("background-color: black;");
}

void WaterfallWidget::pushData(const QVector<float> &data) {
  if (data.isEmpty())
    return;

  // init image on first data with FFT width
  if (img.isNull() || img.width() != data.size()) {
    img = QImage(data.size(), maxRows, QImage::Format_RGB888);
    img.fill(Qt::black);
    nextRow = 0;
    filled = false;
  }

  // convert magnitudes to dB and normalize to 0..1
  QVector<float> norm(data.size());
  float eps = 1e-9f;
  for (int i = 0; i < data.size(); ++i) {
    float db = 20.0f * std::log10(std::max(data[i], eps));
    float t = (db - dBmin) / (dBmax - dBmin); // map dBmin..dBmax -> 0..1
    norm[i] = std::clamp(t, 0.0f, 1.0f);
  }

  appendRow(norm);
  update();
}

void WaterfallWidget::setFrequencyInfo(double centerHz, double sampleHz) {
  centerFrequencyHz = centerHz;
  sampleRateHz = sampleHz;
  markerFrequencies.clear();

  if (sampleRateHz <= 0.0)
    return;

  if (markerCount > 1) {
    markerFrequencies.reserve(markerCount);
    double start = centerFrequencyHz - sampleRateHz / 2.0;
    double step = sampleRateHz / static_cast<double>(markerCount - 1);
    for (int i = 0; i < markerCount; ++i)
      markerFrequencies.append(start + step * static_cast<double>(i));
  } else {
    markerFrequencies.append(centerFrequencyHz);
  }

  update();
}

void WaterfallWidget::reset() {
  img = QImage();
  nextRow = 0;
  filled = false;
  update();
}

void WaterfallWidget::appendRow(const QVector<float> &row) {
  // write one horizontal row into circular buffer
  uchar *scan = img.scanLine(nextRow);
  for (int x = 0; x < row.size(); ++x) {
    uchar c = mapCyan(row[x]);
    scan[3 * x + 0] = 0; // R
    scan[3 * x + 1] = c; // G
    scan[3 * x + 2] = c; // B
  }
  nextRow = (nextRow + 1) % img.height();
  if (nextRow == 0)
    filled = true;
}

void WaterfallWidget::drawFrequencyMarkers(QPainter &painter,
                                           const QRect &targetRect) {
  if (markerFrequencies.isEmpty() || sampleRateHz <= 0.0)
    return;

  painter.save();

  QPen gridPen(QColor(0, 255, 255, 70));
  gridPen.setWidth(1);
  painter.setPen(gridPen);

  const double startFreq = centerFrequencyHz - sampleRateHz / 2.0;
  const double invSpan = 1.0 / sampleRateHz;
  const int left = targetRect.left();
  const int width = targetRect.width();
  const int top = targetRect.top();
  const int bottom = targetRect.bottom();

  QFont labelFont = painter.font();
  labelFont.setPointSizeF(labelFont.pointSizeF() * 0.9);
  painter.setFont(labelFont);
  QFontMetrics metrics(labelFont);

  for (int i = 0; i < markerFrequencies.size(); ++i) {
    double freq = markerFrequencies.at(i);
    double ratio = (freq - startFreq) * invSpan;
    if (ratio < 0.0 || ratio > 1.0)
      continue;

    int x = left + static_cast<int>(std::round(ratio * static_cast<double>(width)));
    painter.drawLine(x, top, x, bottom);

    if (i % 5 == 0) {
      QString text =
          QString::number(freq / 1e6, 'f', 3) + QStringLiteral(" MHz");
      int textWidth = metrics.horizontalAdvance(text);
      int textHeight = metrics.height();
      QRect textRect(x - textWidth / 2, top + 4, textWidth, textHeight);

      painter.save();
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(0, 0, 0, 180));
      QRect bgRect = textRect.adjusted(-4, -2, 4, 2);
      painter.drawRect(bgRect);
      painter.restore();

      painter.save();
      painter.setPen(QColor(0, 255, 255));
      painter.drawText(textRect, Qt::AlignCenter, text);
      painter.restore();

      painter.setPen(gridPen);
    }
  }

  painter.restore();
}
void WaterfallWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  const QRect area = rect();
  p.fillRect(area, Qt::black);

  if (!img.isNull()) {
    // assemble image in correct top->bottom order from circular buffer
    QImage view(img.width(), img.height(), QImage::Format_RGB888);
    if (filled) {
      // bottom part [0..nextRow-1] should be at top
      int topH = img.height() - nextRow;
      memcpy(view.scanLine(0), img.scanLine(nextRow),
             topH * img.bytesPerLine());
      memcpy(view.scanLine(topH), img.scanLine(0),
             nextRow * img.bytesPerLine());
    } else {
      // not filled yet: use [0..nextRow-1]
      view.fill(Qt::black);
      if (nextRow > 0)
        memcpy(view.scanLine(view.height() - nextRow), img.scanLine(0),
               nextRow * img.bytesPerLine());
    }

    // scale smoothly to widget size
    p.drawImage(area, view);
  }

  drawFrequencyMarkers(p, area);
}
