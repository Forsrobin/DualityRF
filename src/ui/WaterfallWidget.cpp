
#include "WaterfallWidget.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

static inline uchar mapCyan(float v) {
  v = std::clamp(v, 0.0f, 1.0f); // 0..1
  int c = int(v * 255.0f + 0.5f);
  return uchar(std::clamp(c, 0, 255));
}

WaterfallWidget::WaterfallWidget(QWidget *parent)
    : QWidget(parent), maxRows(600), nextRow(0), filled(false), dBmin(-60.0f),
      dBmax(0.0f) {
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

void WaterfallWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), Qt::black);
  if (img.isNull())
    return;

  // assemble image in correct top->bottom order from circular buffer
  QImage view(img.width(), img.height(), QImage::Format_RGB888);
  if (filled) {
    // bottom part [0..nextRow-1] should be at top
    int topH = img.height() - nextRow;
    memcpy(view.scanLine(0), img.scanLine(nextRow), topH * img.bytesPerLine());
    memcpy(view.scanLine(topH), img.scanLine(0), nextRow * img.bytesPerLine());
  } else {
    // not filled yet: use [0..nextRow-1]
    view.fill(Qt::black);
    if (nextRow > 0)
      memcpy(view.scanLine(view.height() - nextRow), img.scanLine(0),
             nextRow * img.bytesPerLine());
  }

  // scale smoothly to widget size
  p.drawImage(rect(), view);
}
