
#pragma once
#include <QFile>
#include <QObject>
#include <QThread>
#include <QVector>

class SDRReceiver : public QObject {
  Q_OBJECT
public:
  explicit SDRReceiver(QObject *parent = nullptr);
  ~SDRReceiver();

  // starts the RX thread and keeps it running until app exit or stopStream()
  void startStream(double freqMHz, double sampleRate = 2.6e6);
  void stopStream(); // only used on app shutdown
  void setFftSize(int size);
  void setGainDb(double gainDb);
  void setSampleRate(double sampleRate);
  void setTriggerThresholdDb(double thresholdDb);
  void setCaptureSpanHz(double halfSpanHz); // detection half-span around RX
  void armTriggeredCapture(double preSeconds = 1.0, double postSeconds = 1.0);
  void cancelTriggeredCapture();

public slots:
  // toggles capture without stopping the stream
  void startCapture(const QString &filePath); // writes CF32 interleaved
  void stopCapture();

signals:
  void newFFTData(QVector<float> data);
  void captureCompleted(QString filePath);
  void triggerStatus(bool armed, bool capturing, double centerDb, double thresholdDb, bool above);

private:
  class Worker;
  QThread *thread{nullptr};
  Worker *worker{nullptr};
  bool streaming{false};
  int currentFftSize{4096};
  double currentGainDb{40.0};
  double currentSampleRate{2.6e6};
  double lastFreqMHz{433.81};
};
