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
  void setNoiseLevelDb(double dbfs);
  void setNoiseSpanHz(double halfSpanHz);
  void setTxGainDb(double gainDb);

private:
  class Worker;
  QThread *thread{nullptr};
  Worker *worker{nullptr};
  bool running{false};
  double lastFreqMHz{434.20};
  double lastSampleRate{2.6e6};
  double lastIntensity{0.5};
  double lastNoiseDbfs{-30.0};
  double lastHalfSpanHz{100e3};
  double lastTxGainDb{25.0};
};
