#pragma once
#include <QVector>
#include <QWidget>

class SpectrumWidget : public QWidget {
  Q_OBJECT
public:
  explicit SpectrumWidget(QWidget *parent = nullptr);

public slots:
  void pushData(const QVector<float> &linearMagnitudes);
  void setFrequencyInfo(double centerHz, double sampleRateHz);
  void resetPeaks();
  void setZoomStep(int step);
  void setThresholdDb(double db);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void ensureSize(int n);
  double toDb(float v) const; // v in 0..1

  QVector<float> latest; // linear 0..1
  QVector<float> peak;   // peak hold 0..1
  double centerHz{0.0};
  double sampleRate{0.0};
  float dBmin{-110.0f};
  float dBmax{-10.0f};
  double thresholdDb{std::numeric_limits<double>::quiet_NaN()};
  int zoomStep{0};
  double zoomFactor() const;
};
