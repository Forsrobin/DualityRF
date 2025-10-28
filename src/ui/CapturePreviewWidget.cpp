#include "CapturePreviewWidget.h"
#include "WaterfallWidget.h"
#include <QBoxLayout>
#include <QFile>
#include <QLabel>
#include <QMetaObject>
#include <QTimer>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <complex>
#include <fftw3.h>

// ------------------------ CapturePreviewWorker ------------------------

void CapturePreviewWorker::startPreview(const QString &path, double sampleRateHz,
                                        double rxHz) {
  Q_UNUSED(rxHz);
  running.store(true, std::memory_order_release);

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    emit finished();
    return;
  }

  // FFT params similar to live pipeline
  const int N = 4096;
  std::vector<std::complex<float>> buff(N);
  std::vector<float> window(N, 1.0f);
  // Hann window
  double sumW = 0.0;
  for (int i = 0; i < N; ++i) {
    float w = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * float(i) / float(N - 1)));
    window[i] = w;
    sumW += w;
  }
  float coherentGain = float(sumW / double(N));

  fftwf_complex *in = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * N);
  fftwf_complex *out = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * N);
  fftwf_plan plan = fftwf_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  const float invN = 1.0f / float(N);
  const float ampScale = invN / std::max(coherentGain, 1e-9f);
  // Determine a pacing interval based on sample rate, but faster than real-time
  int sleepMs = 1; // default fast pace
  if (sampleRateHz > 0.0) {
    double frameSec = double(N) / sampleRateHz; // seconds per FFT window
    // Speed up playback: quarter of real-time, but at least 1 ms
    sleepMs = std::max(1, int(frameSec * 1000.0 * 0.25));
  }

  while (running.load(std::memory_order_acquire)) {
    // read one block
    qint64 need = qint64(N) * qint64(sizeof(std::complex<float>));
    qint64 got = f.read(reinterpret_cast<char *>(buff.data()), need);
    if (got < need)
      break;

    // window + FFT
    for (int i = 0; i < N; ++i) {
      float w = window[i];
      in[i][0] = buff[i].real() * w;
      in[i][1] = buff[i].imag() * w;
    }
    fftwf_execute(plan);

    QVector<float> ampsShift(N);
    std::vector<float> amps(N);
    for (int i = 0; i < N; ++i) {
      float re = out[i][0] * ampScale;
      float im = out[i][1] * ampScale;
      amps[i] = std::sqrt(re * re + im * im);
    }
    int half = N / 2;
    for (int i = 0; i < N; ++i)
      ampsShift[i] = amps[(i + half) % N];

    emit frameReady(ampsShift);
    // faster pacing for snappier preview
    QThread::msleep(sleepMs);
  }

  fftwf_destroy_plan(plan);
  fftwf_free(in);
  fftwf_free(out);
  f.close();
  emit finished();
}

void CapturePreviewWorker::stop() { running.store(false, std::memory_order_release); }

// ------------------------ CapturePreviewWidget ------------------------

CapturePreviewWidget::CapturePreviewWidget(const QString &title, QWidget *parent)
    : QFrame(parent) {
  setObjectName("CaptureBox");
  setFrameShape(QFrame::NoFrame);

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
    // page 1: mini waterfall
    waterfall = new WaterfallWidget(this);
    waterfall->setMinimumHeight(160);
    stack->addWidget(waterfall);
  }

  auto *stackHolder = new QWidget(this);
  stackHolder->setLayout(stack);
  outer->addWidget(stackHolder);
  setLayout(outer);

  // Start empty
  stack->setCurrentIndex(0);
}

CapturePreviewWidget::~CapturePreviewWidget() {
  if (thread) {
    if (worker)
      worker->stop();
    thread->quit();
    thread->wait(1000);
    delete thread;
  }
}

void CapturePreviewWidget::setFrequencyInfo(double rxHz_, double centerHz_,
                                            double sampleRateHz_) {
  rxHz = rxHz_;
  centerHz = centerHz_;
  sampleRateHz = sampleRateHz_;
  if (waterfall) {
    waterfall->setFrequencyInfo(centerHz, sampleRateHz);
    waterfall->setRxTxFrequencies(rxHz, 0.0);
  }
}

void CapturePreviewWidget::setCaptureSpanHz(double halfSpanHz) {
  spanHalfHz = halfSpanHz;
  if (waterfall)
    waterfall->setCaptureSpanHz(halfSpanHz);
}

void CapturePreviewWidget::showEmpty() {
  // Stop any ongoing preview generation
  if (thread) {
    if (worker)
      worker->stop();
    thread->quit();
    thread->wait(500);
    delete thread;
    thread = nullptr;
    worker = nullptr;
  }
  if (waterfall)
    waterfall->reset();
  if (stack)
    stack->setCurrentIndex(0);
}

void CapturePreviewWidget::loadFromFile(const QString &filePath) {
  if (!waterfall)
    return;
  waterfall->reset();
  waterfall->setFrequencyInfo(centerHz, sampleRateHz);
  waterfall->setRxTxFrequencies(rxHz, 0.0);
  waterfall->setCaptureSpanHz(spanHalfHz);

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
                              Q_ARG(double, sampleRateHz), Q_ARG(double, rxHz));
  });
  QObject::connect(worker, &CapturePreviewWorker::frameReady, waterfall,
                   &WaterfallWidget::pushData, Qt::QueuedConnection);
  QObject::connect(worker, &CapturePreviewWorker::finished, this, [this]() {
    if (thread) {
      thread->quit();
    }
  });
  QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);

  stack->setCurrentIndex(1);
  thread->start();
}
