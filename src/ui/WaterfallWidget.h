
#pragma once
#include <QImage>
#include <QRect>
#include <QVector>
#include <QWidget>

class QPainter;

class WaterfallWidget : public QWidget {
  Q_OBJECT
public:
  explicit WaterfallWidget(QWidget *parent = nullptr);
public slots:
  void pushData(const QVector<float> &data); // 0..1 or any scale
  void setFrequencyInfo(double centerFrequencyHz, double sampleRateHz);
  void setRxTxFrequencies(double rxHz, double txHz);
  void setZoomStep(int step); // 0 -> 1x, 1 -> 2x, 2 -> 4x, ...
  void setCaptureSpanHz(double halfSpanHz);
  void setShowCaptureSpan(bool show);
  void setNoiseSpanHz(double halfSpanHz);
  void reset();
protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void appendRow(const QVector<float> &row);
  void drawFrequencyMarkers(QPainter &painter, const QRect &targetRect);
  QImage img; // width = fft bins, height = maxRows
  int maxRows;
  int nextRow; // circular write index
  bool filled;
  // simple AGC
  float dBmin, dBmax; // e.g. -60..0 dB window
  double centerFrequencyHz;
  double sampleRateHz;
  double rxFrequencyHz{0.0};
  double txFrequencyHz{0.0};
  double captureSpanHalfHz{100000.0};
  double noiseSpanHalfHz{0.0};
  bool showCaptureSpan{true};
  QVector<double> markerFrequencies;
  static constexpr int markerCount = 40;
  int zoomStep{0}; // 0..N; factor = 2^zoomStep
  double zoomFactor() const;
};
