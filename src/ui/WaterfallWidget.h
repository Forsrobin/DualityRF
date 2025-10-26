
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
  QVector<double> markerFrequencies;
  static constexpr int markerCount = 40;
};
