
#include "WaterfallWidget.h"

WaterfallWidget::WaterfallWidget(QWidget *parent) : QWidget(parent) {
  setMinimumHeight(200);
  setStyleSheet("background-color: black;");
  timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, [this]() { update(); });
  timer->start(100);
}

void WaterfallWidget::pushData(const QVector<float> &data) {
  lines.prepend(data);
  if (lines.size() > 100)
    lines.removeLast();
  update();
}

void WaterfallWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  int h = height() / qMax(1, lines.size());
  for (int y = 0; y < lines.size(); ++y) {
    const auto &row = lines[y];
    int w = width() / qMax(1, row.size());
    for (int x = 0; x < row.size(); ++x) {
      float val = row[x];
      int intensity = std::clamp(int(val * 255), 0, 255);
      p.fillRect(x * w, y * h, w, h, QColor(0, intensity, intensity));
    }
  }
}
