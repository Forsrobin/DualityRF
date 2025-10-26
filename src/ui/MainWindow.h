
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
  void onFftSizeSliderChanged(int sliderStep);
  void onDecreaseFft();
  void onIncreaseFft();

private:
  bool eventFilter(QObject *watched, QEvent *event) override;
  int fftStepToValue(int step) const;
  int clampFftStep(int step) const;
  void applyFftSize(int fftValue);

  static constexpr int kFftMin = 512;
  static constexpr int kFftMax = 8192;
  static constexpr int kFftStep = 512;
  static constexpr int kFftSteps =
      (kFftMax - kFftMin) / kFftStep; // zero-based slider range

  WaterfallWidget *waterfall;
  QDoubleSpinBox *rxFreq;
  QDoubleSpinBox *txFreq;
  QPushButton *startButton;
  QPushButton *unlockButton;
  QPushButton *exitButton;
  QPushButton *fftDecreaseButton;
  QPushButton *fftIncreaseButton;
  QSlider *fftSizeSlider;
  QLabel *fftSizeLabel;
  QLabel *captureStatus1;
  QLabel *captureStatus2;

  SDRReceiver *receiver;
  bool running;
  bool waterfallActive;
  double sampleRateHz;
};
