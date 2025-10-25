
#pragma once
#include "WaterfallWidget.h"
#include <QMainWindow>

class QDoubleSpinBox;
class QPushButton;
class QLabel;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);

private slots:
  void onStart();
  void onStateUpdate();

private:
  WaterfallWidget *waterfall;
  QDoubleSpinBox *rxFreq;
  QDoubleSpinBox *txFreq;
  QPushButton *startButton;
  QPushButton *unlockButton;
  QLabel *captureStatus1;
  QLabel *captureStatus2;
  bool running;
};
