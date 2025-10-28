#include "CapturePreviewWidget.h"
#include "WaterfallWidget.h"
#include "WaveformWidget.h"
#include <QBoxLayout>
#include <QFile>
#include <QLabel>
#include <QMetaObject>
#include <QTimer>
#include <QStyle>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <complex>
#include <fftw3.h>

// ------------------------ CapturePreviewWorker ------------------------

void CapturePreviewWorker::startPreview(const QString &path, double sampleRateHz) {
  running.store(true, std::memory_order_release);

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    emit finished();
    return;
  }

  // Compute a compact time waveform (amplitude envelope) once, no looping
  qint64 totalBytes = f.size();
  qint64 totalSamples = totalBytes / qint64(sizeof(std::complex<float>));
  if (totalSamples <= 0) {
    emit finished();
    return;
  }
  const int targetPoints = 1500; // resolution of the preview
  int bins = std::min<qint64>(targetPoints, totalSamples);
  QVector<float> env(bins, 0.0f);

  // Aggregate max magnitude over contiguous chunks to fill env
  qint64 chunk = 8192; // samples per read
  std::vector<std::complex<float>> buff;
  buff.resize(static_cast<size_t>(chunk));
  qint64 processed = 0;
  while (running.load(std::memory_order_acquire) && processed < totalSamples) {
    qint64 toRead = std::min<qint64>(chunk, totalSamples - processed);
    qint64 got = f.read(reinterpret_cast<char *>(buff.data()),
                        toRead * qint64(sizeof(std::complex<float>)));
    if (got <= 0)
      break;
    qint64 ns = got / qint64(sizeof(std::complex<float>));
    for (qint64 i = 0; i < ns; ++i) {
      float re = buff[size_t(i)].real();
      float im = buff[size_t(i)].imag();
      float a = std::sqrt(re * re + im * im);
      // Map sample index to preview bin
      qint64 idx = processed + i;
      int b = int((idx * bins) / totalSamples);
      if (b < 0)
        b = 0;
      else if (b >= bins)
        b = bins - 1;
      env[b] = std::max(env[b], a);
    }
    processed += ns;
  }
  f.close();

  // Normalize to 0..1
  float mx = 1e-9f;
  for (float v : env)
    mx = std::max(mx, v);
  if (mx > 0.0f) {
    for (float &v : env)
      v = std::min(1.0f, v / mx);
  }

  // Simple peak picking: local maxima, keep strongest up to limit
  QVector<int> peaks;
  for (int i = 1; i + 1 < env.size(); ++i) {
    if (env[i] > env[i - 1] && env[i] > env[i + 1])
      peaks.push_back(i);
  }
  // Reduce clutter: drop weak peaks
  peaks.erase(std::remove_if(peaks.begin(), peaks.end(), [&](int i) {
                 return env[i] < 0.2f; // keep only >= 20% of max
               }),
              peaks.end());
  // Keep top N by amplitude
  const int maxPeaks = 32;
  std::stable_sort(peaks.begin(), peaks.end(), [&](int a, int b) {
    return env[a] > env[b];
  });
  if (peaks.size() > maxPeaks)
    peaks.resize(maxPeaks);
  // Resort by position for drawing
  std::sort(peaks.begin(), peaks.end());

  double durationSec = (sampleRateHz > 0.0) ? (double(totalSamples) / sampleRateHz)
                                            : 0.0;
  emit waveformReady(env, durationSec, peaks);
  emit finished();
}

void CapturePreviewWorker::stop() { running.store(false, std::memory_order_release); }

// ------------------------ CapturePreviewWidget ------------------------

CapturePreviewWidget::CapturePreviewWidget(const QString &title, QWidget *parent)
    : QFrame(parent) {
  setObjectName("CaptureBox");
  setFrameShape(QFrame::NoFrame);
  // default state
  setProperty("completed", false);

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(8, 8, 8, 8);
  outer->setSpacing(6);

  titleLabel = new QLabel(title, this);
  titleLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  outer->addWidget(titleLabel);

  stack = new QStackedLayout();
  {
    // page 0: empty state label
    emptyLabel = new QLabel("EMPTY", this);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setMinimumHeight(160);
    stack->addWidget(emptyLabel);
  }
  {
    // page 1: waveform preview
    waveform = new WaveformWidget(this);
    stack->addWidget(waveform);
  }

  auto *stackHolder = new QWidget(this);
  stackHolder->setLayout(stack);
  outer->addWidget(stackHolder);
  setLayout(outer);

  // Start empty
  stack->setCurrentIndex(0);
}

CapturePreviewWidget::~CapturePreviewWidget() { showEmpty(); }

void CapturePreviewWidget::setFrequencyInfo(double rxHz_, double centerHz_,
                                            double sampleRateHz_) {
  rxHz = rxHz_;
  centerHz = centerHz_;
  sampleRateHz = sampleRateHz_;
  // No-op for waveform visualization; keep for consistency
}

void CapturePreviewWidget::setCaptureSpanHz(double halfSpanHz) {
  spanHalfHz = halfSpanHz;
  // No visualization overlay for waveform preview
}

void CapturePreviewWidget::showEmpty() {
  // Stop any ongoing preview generation
  if (thread) {
    if (worker) {
      // Disconnect all to avoid callbacks during teardown
      QObject::disconnect(worker, nullptr, nullptr, nullptr);
      worker->stop();
    }
    QObject::disconnect(thread, nullptr, nullptr, nullptr);
    thread->quit();
    // Wait longer to ensure worker exits cleanly
    thread->wait(2000);
    if (worker) {
      delete worker;
      worker = nullptr;
    }
    delete thread;
    thread = nullptr;
  }
  // clear
  if (stack)
    stack->setCurrentIndex(0);
  setCompleted(false);
}

void CapturePreviewWidget::loadFromFile(const QString &filePath) {
  // proceed to load waveform from file
  // reset waveform view
  if (waveform)
    waveform->setData(QVector<float>{}, 0.0, QVector<int>{});

  // Ensure any prior worker is stopped
  if (thread) {
    if (worker)
      worker->stop();
    thread->quit();
    thread->wait(500);
    delete thread;
    thread = nullptr;
    worker = nullptr;
  }

  thread = new QThread(this);
  worker = new CapturePreviewWorker();
  worker->moveToThread(thread);
  QObject::connect(thread, &QThread::started, [this, filePath]() {
    QMetaObject::invokeMethod(worker, "startPreview", Qt::QueuedConnection,
                              Q_ARG(QString, filePath),
                              Q_ARG(double, sampleRateHz));
  });
  QObject::connect(worker, &CapturePreviewWorker::waveformReady, waveform,
                   &WaveformWidget::setData, Qt::QueuedConnection);
  // When finished, quit the thread loop (safe even if already requested)
  QObject::connect(worker, &CapturePreviewWorker::finished, thread,
                   &QThread::quit);

  stack->setCurrentIndex(1);
  thread->start();
}

void CapturePreviewWidget::setCompleted(bool on) {
  completed = on;
  setProperty("completed", completed);
  // refresh style to apply dynamic property selector
  style()->unpolish(this);
  style()->polish(this);
  update();
}
