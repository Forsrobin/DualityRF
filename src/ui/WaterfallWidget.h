
#pragma once
#include <QImage>
#include <QVector>
#include <QWidget>

class WaterfallWidget : public QWidget {
  Q_OBJECT
public:
  explicit WaterfallWidget(QWidget *parent = nullptr);
public slots:
  void pushData(const QVector<float> &data); // 0..1 or any scale
protected:
  void paintEvent(QPaintEvent *event) override;

private:
  void appendRow(const QVector<float> &row);
  QImage img; // width = fft bins, height = maxRows
  int maxRows;
  int nextRow; // circular write index
  bool filled;
  // simple AGC
  float dBmin, dBmax; // e.g. -60..0 dB window
};
