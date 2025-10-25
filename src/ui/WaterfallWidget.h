
#pragma once
#include <QPainter>
#include <QTimer>
#include <QWidget>

class WaterfallWidget : public QWidget {
  Q_OBJECT
public:
  explicit WaterfallWidget(QWidget *parent = nullptr);
  void pushData(const QVector<float> &data);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QVector<QVector<float>> lines;
  QTimer *timer;
};
