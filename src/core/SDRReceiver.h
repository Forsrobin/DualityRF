
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
  void startStream(double freqMHz, double sampleRate = 2.8e6);
  void stopStream(); // only used on app shutdown
  void setFftSize(int size);

public slots:
  // toggles capture without stopping the stream
  void startCapture(const QString &filePath); // writes CF32 interleaved
  void stopCapture();

signals:
  void newFFTData(QVector<float> data);

private:
  class Worker;
  QThread *thread{nullptr};
  Worker *worker{nullptr};
  bool streaming{false};
  int currentFftSize{4096};
};
