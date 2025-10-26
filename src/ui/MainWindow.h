
#pragma once
#include "../core/SDRReceiver.h"
#include "WaterfallWidget.h"
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>

class QCloseEvent;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  void startWaterfall();
  explicit MainWindow(QWidget *parent = nullptr);

protected:
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onStart();
  void onStateUpdate();
  void onRxFrequencyChanged(double frequencyMHz);
  void onExitRequested();
  void onZoomSliderChanged(int sliderStep);
  void onZoomOut();
  void onZoomIn();

private:
  bool eventFilter(QObject *watched, QEvent *event) override;
  int clampZoomStep(int step) const;
  void applyZoomStep(int step);
  static constexpr int kZoomMinStep = 0; // 1x
  static constexpr int kZoomMaxStep = 4; // 16x

  WaterfallWidget *waterfall;
  QDoubleSpinBox *rxFreq;
  QDoubleSpinBox *txFreq;
  QPushButton *startButton;
  QPushButton *unlockButton;
  QPushButton *exitButton;
  QPushButton *zoomOutButton;
  QPushButton *zoomInButton;
  QSlider *zoomSlider;
  QLabel *zoomLabel;
  QLabel *captureStatus1;
  QLabel *captureStatus2;

  SDRReceiver *receiver;
  bool running;
  bool waterfallActive;
  double sampleRateHz;
};
