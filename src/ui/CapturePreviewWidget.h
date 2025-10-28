#pragma once
#include <QObject>
#include <QFrame>
#include <QLabel>
#include <QStackedLayout>
#include <QThread>
#include <QVector>
#include <QString>
#include <atomic>
#include <complex>

class WaterfallWidget;
class WaveformWidget;

// Background worker that reads a CF32 file and emits FFT frames
class CapturePreviewWorker : public QObject {
  Q_OBJECT
public:
  explicit CapturePreviewWorker(QObject *parent = nullptr) : QObject(parent) {}
public slots:
  void startPreview(const QString &path, double sampleRateHz);
  void stop();
signals:
  void waveformReady(QVector<float> data, double durationSec,
                     QVector<int> peakIndices);
  void finished();
private:
  std::atomic<bool> running{false};
};

// A boxed panel with a title and either an EMPTY label or a mini waterfall
class CapturePreviewWidget : public QFrame {
  Q_OBJECT
public:
  explicit CapturePreviewWidget(const QString &title, QWidget *parent = nullptr);
  ~CapturePreviewWidget() override;

  void setFrequencyInfo(double rxHz, double centerHz, double sampleRateHz);
  void setCaptureSpanHz(double halfSpanHz);
  void setCompleted(bool on);

public slots:
  void showEmpty();
  void loadFromFile(const QString &filePath);

private:
  QLabel *titleLabel{nullptr};
  QStackedLayout *stack{nullptr};
  QLabel *emptyLabel{nullptr};
  // replaced mini waterfall with waveform
  WaveformWidget *waveform{nullptr};

  // Preview thread + worker
  QThread *thread{nullptr};
  CapturePreviewWorker *worker{nullptr};

  // For configuring the waterfall view
  double rxHz{0.0};
  double centerHz{0.0};
  double sampleRateHz{0.0};
  double spanHalfHz{100000.0};
  bool completed{false};
};
