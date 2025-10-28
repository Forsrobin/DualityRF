
#include "WaterfallWidget.h"
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QString>
#include <algorithm>
#include <cmath>

static inline void mapHeat(float v, uchar &r, uchar &g, uchar &b) {
  // 0..1: dark teal -> cyan -> yellow -> red
  v = std::clamp(v, 0.0f, 1.0f);
  float rF = 0.0f, gF = 0.0f, bF = 0.0f;

  if (v < 0.6f) {
    // 0.0 .. 0.6: dark teal (0,60,60) to cyan (0,255,255)
    float t = v / 0.6f;
    rF = 0.0f;
    gF = 60.0f + t * (255.0f - 60.0f);
    bF = 60.0f + t * (255.0f - 60.0f);
  } else if (v < 0.85f) {
    // 0.6 .. 0.85: cyan to yellow (255,255,0)
    float t = (v - 0.6f) / 0.25f;
    rF = t * 255.0f;
    gF = 255.0f;
    bF = (1.0f - t) * 255.0f;
  } else {
    // 0.85 .. 1.0: yellow to red (255,0,0)
    float t = (v - 0.85f) / 0.15f;
    rF = 255.0f;
    gF = (1.0f - t) * 255.0f;
    bF = 0.0f;
  }
  r = uchar(std::clamp(int(rF + 0.5f), 0, 255));
  g = uchar(std::clamp(int(gF + 0.5f), 0, 255));
  b = uchar(std::clamp(int(bF + 0.5f), 0, 255));
}

WaterfallWidget::WaterfallWidget(QWidget *parent)
    : QWidget(parent), maxRows(600), nextRow(0), filled(false), dBmin(-110.0f),
      dBmax(-10.0f), centerFrequencyHz(0.0), sampleRateHz(0.0) {
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

void WaterfallWidget::setRxTxFrequencies(double rxHz, double txHz) {
  rxFrequencyHz = rxHz;
  txFrequencyHz = txHz;
  update();
}

void WaterfallWidget::setCaptureSpanHz(double halfSpanHz) {
  if (halfSpanHz < 0.0)
    halfSpanHz = 0.0;
  captureSpanHalfHz = halfSpanHz;
  update();
}

void WaterfallWidget::setShowCaptureSpan(bool show) {
  if (showCaptureSpan != show) {
    showCaptureSpan = show;
    update();
  }
}

void WaterfallWidget::setNoiseSpanHz(double halfSpanHz) {
  if (halfSpanHz < 0.0)
    halfSpanHz = 0.0;
  if (std::abs(noiseSpanHalfHz - halfSpanHz) > 1e-6) {
    noiseSpanHalfHz = halfSpanHz;
    update();
  }
}

void WaterfallWidget::reset() {
  img = QImage();
  nextRow = 0;
  filled = false;
  update();
}

double WaterfallWidget::zoomFactor() const {
  if (zoomStep <= 0)
    return 1.0;
  double f = 1.0;
  for (int i = 0; i < zoomStep; ++i)
    f *= 2.0;
  return f;
}

void WaterfallWidget::setZoomStep(int step) {
  if (step < 0)
    step = 0;
  if (step != zoomStep) {
    zoomStep = step;
    update();
  }
}

void WaterfallWidget::appendRow(const QVector<float> &row) {
  // write one horizontal row into circular buffer
  uchar *scan = img.scanLine(nextRow);
  for (int x = 0; x < row.size(); ++x) {
    uchar r, g, b;
    mapHeat(row[x], r, g, b);
    scan[3 * x + 0] = r; // R
    scan[3 * x + 1] = g; // G
    scan[3 * x + 2] = b; // B
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

  const double span = sampleRateHz / zoomFactor();
  const double startFreq = centerFrequencyHz - span / 2.0;
  const double invSpan = 1.0 / span;
  const int left = targetRect.left();
  const int width = targetRect.width();
  const int top = targetRect.top();
  const int bottom = targetRect.bottom();

  // Intentionally skip frequency grid/labels on the waterfall to avoid
  // duplication with the spectrum widget. Only draw RX/TX and capture span.

  QFont labelFont = painter.font();
  labelFont.setPointSizeF(labelFont.pointSizeF() * 0.9);
  painter.setFont(labelFont);
  QFontMetrics metrics(labelFont);

  // RX/TX guide lines and capture span
  auto drawGuide = [&](double freq, const QColor &color, const QString &label) {
    if (freq <= 0.0)
      return;
    double ratio = (freq - startFreq) * invSpan;
    if (ratio < 0.0 || ratio > 1.0)
      return;
    int x = left + static_cast<int>(std::round(ratio * static_cast<double>(width)));
    QPen pen(color);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.drawLine(x, top, x, bottom);

    QString text = label + QString(": ") + QString::number(freq / 1e6, 'f', 3) + " MHz";
    int tw = metrics.horizontalAdvance(text);
    int th = metrics.height();
    QRect tr(x - tw / 2, bottom - th - 4, tw, th);

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRect(tr.adjusted(-4, -2, 4, 2));
    painter.restore();

    painter.save();
    painter.setPen(color);
    painter.drawText(tr, Qt::AlignCenter, text);
    painter.restore();
    painter.setPen(gridPen);
  };

  drawGuide(rxFrequencyHz, QColor(255, 255, 0), QStringLiteral("RX"));
  drawGuide(txFrequencyHz, QColor(255, 80, 80), QStringLiteral("TX"));

  // Capture span visualization around RX: ±captureSpanHalfHz
  if (showCaptureSpan && rxFrequencyHz > 0.0 && captureSpanHalfHz > 0.0) {
    double span = sampleRateHz / zoomFactor();
    double startFreq = centerFrequencyHz - span / 2.0;
    double invSpan = 1.0 / span;
    int left = targetRect.left();
    int width = targetRect.width();
    int top = targetRect.top();
    int bottom = targetRect.bottom();

    double fL = rxFrequencyHz - captureSpanHalfHz;
    double fR = rxFrequencyHz + captureSpanHalfHz;
    double rL = (fL - startFreq) * invSpan;
    double rR = (fR - startFreq) * invSpan;
    if (!(rL < 0.0 && rR < 0.0) && !(rL > 1.0 && rR > 1.0)) {
      int xL = left + static_cast<int>(std::round(std::clamp(rL, 0.0, 1.0) * width));
      int xR = left + static_cast<int>(std::round(std::clamp(rR, 0.0, 1.0) * width));
      if (xL > xR)
        std::swap(xL, xR);
      // Draw boundary lines
      QPen spanPen(QColor(0, 255, 0));
      spanPen.setWidth(2);
      painter.setPen(spanPen);
      painter.drawLine(xL, top, xL, bottom);
      painter.drawLine(xR, top, xR, bottom);
      // Fill translucent
      painter.save();
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(0, 255, 0, 40));
      painter.drawRect(QRect(QPoint(xL, top), QPoint(xR, bottom)));
      painter.restore();
    }
    painter.setPen(gridPen);
  }

  // Noise span visualization around TX: ±noiseSpanHalfHz
  if (txFrequencyHz > 0.0 && noiseSpanHalfHz > 0.0) {
    double span = sampleRateHz / zoomFactor();
    double startFreq = centerFrequencyHz - span / 2.0;
    double invSpan = 1.0 / span;
    int left = targetRect.left();
    int width = targetRect.width();
    int top = targetRect.top();
    int bottom = targetRect.bottom();

    double fL = txFrequencyHz - noiseSpanHalfHz;
    double fR = txFrequencyHz + noiseSpanHalfHz;
    double rL = (fL - startFreq) * invSpan;
    double rR = (fR - startFreq) * invSpan;
    if (!(rL < 0.0 && rR < 0.0) && !(rL > 1.0 && rR > 1.0)) {
      int xL = left + static_cast<int>(std::round(std::clamp(rL, 0.0, 1.0) * width));
      int xR = left + static_cast<int>(std::round(std::clamp(rR, 0.0, 1.0) * width));
      if (xL > xR)
        std::swap(xL, xR);
      // Draw boundary lines in red
      QPen spanPen(QColor(255, 80, 80));
      spanPen.setWidth(2);
      painter.setPen(spanPen);
      painter.drawLine(xL, top, xL, bottom);
      painter.drawLine(xR, top, xR, bottom);
      // Fill translucent red
      painter.save();
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(255, 80, 80, 40));
      painter.drawRect(QRect(QPoint(xL, top), QPoint(xR, bottom)));
      painter.restore();
    }
    painter.setPen(gridPen);
  }

  painter.restore();
}
void WaterfallWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  const QRect area = rect();
  p.fillRect(area, Qt::black);

  // Match spectrum's horizontal plot margins to keep alignment exact
  const int leftMargin = 52;  // keep in sync with SpectrumWidget
  const int rightMargin = 8;
  const QRect plotRect = area.adjusted(leftMargin, 0, -rightMargin, 0);

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

    // apply horizontal zoom by cropping central ROI and scaling to widget
    double z = zoomFactor();
    int srcW = std::max(1, int(std::round(double(view.width()) / z)));
    int srcX = (view.width() - srcW) / 2;
    QRect srcRect(srcX, 0, srcW, view.height());
    p.drawImage(plotRect, view, srcRect);
  }

  drawFrequencyMarkers(p, plotRect);
}
