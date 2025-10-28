#pragma once

#include <QObject>
#include <QThread>

class SDRTransmitter : public QObject {
  Q_OBJECT
public:
  explicit SDRTransmitter(QObject *parent = nullptr);
  ~SDRTransmitter();

  void start();
  void stop();

  void setFrequencyMHz(double freqMHz);
  void setSampleRate(double sampleRate);
  void setNoiseIntensity(double intensity01);
  void setNoiseSpanHz(double halfSpanHz);

private:
  class Worker;
  QThread *thread{nullptr};
  Worker *worker{nullptr};
  bool running{false};
  double lastFreqMHz{433.95};
  double lastSampleRate{2.6e6};
  double lastIntensity{0.5};
  double lastHalfSpanHz{100e3};
};

