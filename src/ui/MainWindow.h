
#pragma once
#include "../core/SDRReceiver.h"
#include "WaterfallWidget.h"
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>

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

  SDRReceiver *receiver;
  bool running;
};
