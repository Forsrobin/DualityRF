
#pragma once
#include "../core/SDRReceiver.h"
#include "WaterfallWidget.h"
#include "SpectrumWidget.h"
#include "CapturePreviewWidget.h"
#include "componenets/InfoDialog.h"
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>

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
  void onGainChanged(int sliderValue);
  void onSampleRateChanged(int index);
  void onThresholdChanged(int sliderValue);
  void onSpanChanged(int sliderValue);
  void onCaptureCompleted(const QString &filePath);
  void onTriggerStatus(bool armed, bool capturing, double centerDb, double thresholdDb, bool above);
  void onDetectorModeChanged(int index);
  void onDwellChanged(double seconds);
  void onAvgTauChanged(double seconds);

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
  QPushButton *infoButton;
  QPushButton *zoomOutButton;
  QPushButton *zoomInButton;
  QSlider *zoomSlider;
  QLabel *zoomLabel;
  QSlider *gainSlider;
  QLabel *gainLabel;
  QSlider *spanSlider;
  QLabel *spanLabel;
  QComboBox *sampleRateCombo;
  SpectrumWidget *spectrum;
  QLabel *captureStatus1;
  QLabel *captureStatus2;
  CapturePreviewWidget *captureBox1{nullptr};
  CapturePreviewWidget *captureBox2{nullptr};
  QSlider *thresholdSlider;
  QLabel *thresholdLabel;
  QLabel *triggerStatusLabel;
  QComboBox *detectorModeCombo;
  QDoubleSpinBox *dwellSpin;
  QDoubleSpinBox *avgTauSpin;
  InfoDialog *infoDialog{nullptr};

  SDRReceiver *receiver;
  bool running;
  bool waterfallActive;
  double sampleRateHz;

  // Track sequential capture states
  bool capture1Done{false};
  bool capture2Done{false};
};
