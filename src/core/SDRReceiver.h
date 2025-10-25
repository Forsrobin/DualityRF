
#pragma once
#include <QObject>
#include <QThread>
#include <QVector>
#include <SoapySDR/Device.hpp>
#include <atomic>

class SDRReceiver : public QObject {
  Q_OBJECT
public:
  explicit SDRReceiver(QObject *parent = nullptr);
  ~SDRReceiver();

  void start(double freqMHz, double sampleRate = 2.4e6);
  void stop();

signals:
  void newFFTData(QVector<float> data);

private:
  void run();

  QThread worker;
  std::atomic<bool> running;
  double freq;
  double rate;
};
