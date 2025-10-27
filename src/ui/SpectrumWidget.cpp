#include "SpectrumWidget.h"
#include <QPainter>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>
#include <limits>

SpectrumWidget::SpectrumWidget(QWidget *parent) : QWidget(parent) {
  setMinimumHeight(180);
}

void SpectrumWidget::ensureSize(int n) {
  if (latest.size() != n) {
    latest.resize(n);
    peak.resize(n);
    std::fill(peak.begin(), peak.end(), 0.0f);
  }
}

double SpectrumWidget::toDb(float v) const {
  float eps = 1e-9f;
  float db = 20.0f * std::log10(std::max(v, eps));
  return std::clamp(db, dBmin, dBmax);
}

void SpectrumWidget::pushData(const QVector<float> &linearMagnitudes) {
  if (linearMagnitudes.isEmpty())
    return;
  ensureSize(linearMagnitudes.size());
  latest = linearMagnitudes;
  for (int i = 0; i < latest.size(); ++i)
    peak[i] = std::max(peak[i], latest[i]);
  update();
}

void SpectrumWidget::setFrequencyInfo(double cHz, double srHz) {
  centerHz = cHz;
  sampleRate = srHz;
  update();
}

void SpectrumWidget::resetPeaks() {
  std::fill(peak.begin(), peak.end(), 0.0f);
  update();
}

double SpectrumWidget::zoomFactor() const {
  if (zoomStep <= 0)
    return 1.0;
  double f = 1.0;
  for (int i = 0; i < zoomStep; ++i)
    f *= 2.0;
  return f;
}

void SpectrumWidget::setZoomStep(int step) {
  if (step < 0)
    step = 0;
  if (zoomStep != step) {
    zoomStep = step;
    update();
  }
}

void SpectrumWidget::setThresholdDb(double db) {
  thresholdDb = db;
  update();
}

void SpectrumWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(0, 0, 0));
  if (latest.isEmpty())
    return;

  const QRect r = rect().adjusted(52, 8, -8, -36); // more left margin for first label
  // axes and grid
  p.setPen(QColor(0, 255, 255, 60));
  for (int d = int(dBmin / 10) * 10; d <= dBmax; d += 10) {
    double t = (double(d) - dBmin) / (dBmax - dBmin);
    int y = r.bottom() - int(std::round(t * r.height()));
    p.drawLine(r.left(), y, r.right(), y);
    p.setPen(QColor(0, 255, 255, 180));
    p.drawText(2, y + 4, QString::number(d));
    p.setPen(QColor(0, 255, 255, 60));
  }

  // draw latest trace
  auto drawTrace = [&](const QVector<float> &data, const QColor &color) {
    p.setPen(color);
    const int n = data.size();
    int srcN = std::max(2, int(std::round(double(n) / zoomFactor())));
    int start = (n - srcN) / 2;
    for (int k = 1; k < srcN; ++k) {
      int i0 = start + (k - 1);
      int i1 = start + k;
      double d0 = toDb(data[i0]);
      double d1 = toDb(data[i1]);
      double t0 = (d0 - dBmin) / (dBmax - dBmin);
      double t1 = (d1 - dBmin) / (dBmax - dBmin);
      int x0 = r.left() + (k - 1) * r.width() / (srcN - 1);
      int x1 = r.left() + k * r.width() / (srcN - 1);
      int y0 = r.bottom() - int(std::round(t0 * r.height()));
      int y1 = r.bottom() - int(std::round(t1 * r.height()));
      p.drawLine(x0, y0, x1, y1);
    }
  };

  drawTrace(peak, QColor(255, 180, 0));     // orange peak hold
  drawTrace(latest, QColor(0, 255, 255));   // cyan live

  // Draw threshold line if set
  if (!std::isnan(thresholdDb)) {
    double t = (thresholdDb - dBmin) / (dBmax - dBmin);
    int y = r.bottom() - int(std::round(t * r.height()));
    QPen thrPen(QColor(255, 255, 0, 200));
    thrPen.setStyle(Qt::DashLine);
    thrPen.setWidth(1);
    p.setPen(thrPen);
    p.drawLine(r.left(), y, r.right(), y);
    p.setPen(QColor(255, 255, 0));
    p.drawText(r.right() - 140, y - 2, 136, 14, Qt::AlignRight,
               QString("Thr: %1 dB").arg(thresholdDb, 0, 'f', 0));
  }

  // frequency labels
  if (sampleRate > 0.0) {
    double span = sampleRate / zoomFactor();
    double startFreq = centerHz - span / 2.0;
    int major = std::max(12, r.width() / 70); // denser labels
    p.setPen(QColor(0, 255, 255));
    QFontMetrics fm(p.font());
    int lastRight = r.left() - 4;
    for (int i = 0; i <= major; ++i) {
      double f = startFreq + (span * i / double(major));
      int x = r.left() + int(std::round(double(i) * r.width() / double(major)));
      // small tick
      p.drawLine(x, r.bottom(), x, r.bottom() - 6);
      // label with clamping to visible region
      QString text = QString::number(f / 1e6, 'f', 3) + " MHz";
      int tw = fm.horizontalAdvance(text);
      int leftX = x - tw / 2;
      if (leftX < r.left()) leftX = r.left();
      int rightX = leftX + tw;
      if (rightX > r.right()) { rightX = r.right(); leftX = rightX - tw; }
      if (leftX <= lastRight + 2) { // skip overlapping
        continue;
      }
      p.drawText(leftX, r.bottom() + 18, text);
      lastRight = rightX;
    }
  }

  // Peak dB readout
  double maxDb = dBmin;
  for (float v : latest)
    maxDb = std::max(maxDb, toDb(v));
  p.setPen(QColor(255, 255, 0));
  p.drawText(r.right() - 120, r.top() + 2, 118, 16, Qt::AlignRight,
             QString("Peak: %1 dB").arg(maxDb, 0, 'f', 1));
}
