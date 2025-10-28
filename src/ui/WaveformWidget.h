#pragma once
#include <QWidget>
#include <QVector>

class WaveformWidget : public QWidget {
  Q_OBJECT
public:
  explicit WaveformWidget(QWidget *parent = nullptr);

public slots:
  void setData(const QVector<float> &samples, double durationSec,
               const QVector<int> &peakIndices);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QVector<float> data;        // normalized 0..1
  QVector<int> peaks;         // indices into data
  double durationSeconds{0.0};
};
