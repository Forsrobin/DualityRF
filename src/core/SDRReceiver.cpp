
#include "SDRReceiver.h"
#include <QMetaType>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <fftw3.h>
#include <vector>

class SDRReceiver::Worker : public QObject {
  Q_OBJECT
public:
  Worker() = default;
  ~Worker() override { closeDevice(); }

public slots:
  void configure(double freqMHz, double sampleRate) {
    // Route to thread-safe immediate setter
    configureImmediate(freqMHz, sampleRate);
  }

  void startWork() {
    running = true;
    activeFftSize =
        std::clamp(requestedFftSize.load(std::memory_order_acquire), 512, 8192);
    std::vector<std::complex<float>> buff(activeFftSize);

    while (running) {
      if (QThread::currentThread()->isInterruptionRequested())
        running = false;
      if (!dev)
        openDevice(); // lazy open
      if (!dev) {
        QThread::msleep(200);
        continue;
      }

      int desired =
          std::clamp(requestedFftSize.load(std::memory_order_acquire), 512, 8192);
      if (desired != activeFftSize) {
        activeFftSize = desired;
        buff.assign(activeFftSize, std::complex<float>{});
        ensureFFTW(activeFftSize);
      }

      void *buffs[] = {buff.data()};

      // small timeout to stay responsive on stop/capture toggles
      int flags;
      long long timeNs;
      int ret =
          dev->readStream(stream, buffs, activeFftSize, flags, timeNs, 10'000);
      if (ret <= 0)
        continue;
      if (ret != activeFftSize)
        continue;

      // FFT
      ensureFFTW(activeFftSize);
      for (int i = 0; i < activeFftSize; ++i) {
        in[i][0] = buff[i].real();
        in[i][1] = buff[i].imag();
      }
      fftwf_execute(plan);

      QVector<float> mags(activeFftSize);
      float maxv = 1e-9f;
      for (int i = 0; i < activeFftSize; ++i) {
        float re = out[i][0], im = out[i][1];
        float m = std::sqrt(re * re + im * im);
        if (m > maxv)
          maxv = m;
        mags[i] = m;
      }
      float inv = 1.0f / maxv;
      for (int i = 0; i < activeFftSize; ++i)
        mags[i] *= inv;
      emit newFFTData(mags);

      // optional capture
      if (capturing && file.isOpen()) {
        file.write(reinterpret_cast<const char *>(buff.data()),
                   ret * sizeof(std::complex<float>));
      }

      if (reconfigureRequested.exchange(false)) {
        // Consume latest pending config
        double f = pendingFreqHz.load(std::memory_order_acquire);
        double r = pendingRate.load(std::memory_order_acquire);
        freqHz = f;
        rate = r;
        applyTuning();
      }
    }
    closeDevice();
    freeFFTW();
  }

  void stopWork() { running = false; }

  void beginCapture(const QString &path) {
    if (file.isOpen())
      file.close();
    file.setFileName(path);
    if (!file.open(QIODevice::WriteOnly)) {
      capturing = false;
      return;
    }
    capturing = true;
  }
  void endCapture() {
    capturing = false;
    if (file.isOpen())
      file.close();
  }
  void updateFftSize(int size) {
    int clamped = std::clamp(size, 512, 8192);
    requestedFftSize.store(clamped, std::memory_order_release);
  }

  // Thread-safe: may be called from any thread
  void configureImmediate(double freqMHz, double sampleRate) {
    pendingFreqHz.store(freqMHz * 1e6, std::memory_order_release);
    pendingRate.store(sampleRate, std::memory_order_release);
    reconfigureRequested.store(true, std::memory_order_release);
  }

private:
  void openDevice() {
    try {
      SoapySDR::Kwargs args;
      args["driver"] = "rtlsdr";
      dev = SoapySDR::Device::make(args);
      stream = dev->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
      applyTuning();
      dev->activateStream(stream);
    } catch (...) {
      dev = nullptr;
      stream = nullptr;
    }
  }
  void applyTuning() {
    if (!dev)
      return;
    dev->setSampleRate(SOAPY_SDR_RX, 0, rate);
    dev->setFrequency(SOAPY_SDR_RX, 0, freqHz);
    dev->setGain(SOAPY_SDR_RX, 0, 40);
  }
  void closeDevice() {
    if (dev) {
      if (stream) {
        dev->deactivateStream(stream);
        dev->closeStream(stream);
        stream = nullptr;
      }
      SoapySDR::Device::unmake(dev);
      dev = nullptr;
    }
  }
  void ensureFFTW(int N) {
    if (sz == N)
      return;
    freeFFTW();
    sz = N;
    in = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * sz);
    out = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * sz);
    plan = fftwf_plan_dft_1d(sz, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  }
  void freeFFTW() {
    if (plan)
      fftwf_destroy_plan(plan);
    if (in)
      fftwf_free(in);
    if (out)
      fftwf_free(out);
    plan = nullptr;
    in = out = nullptr;
    sz = 0;
  }

  // state
  std::atomic<bool> running{false};
  std::atomic<bool> capturing{false};
  std::atomic<bool> reconfigureRequested{false};

  double freqHz{433.81e6};
  double rate{2.6e6};
  std::atomic<double> pendingFreqHz{433.81e6};
  std::atomic<double> pendingRate{2.6e6};

  SoapySDR::Device *dev{nullptr};
  SoapySDR::Stream *stream{nullptr};

  // FFT
  int sz{0};
  fftwf_complex *in{nullptr}, *out{nullptr};
  fftwf_plan plan{nullptr};
  std::atomic<int> requestedFftSize{4096};
  int activeFftSize{4096};

  QFile file;

signals:
  void newFFTData(QVector<float> data);
};

SDRReceiver::SDRReceiver(QObject *parent) : QObject(parent) {
  qRegisterMetaType<QVector<float>>("QVector<float>");
}
SDRReceiver::~SDRReceiver() { stopStream(); }

void SDRReceiver::startStream(double freqMHz, double sampleRate) {
  if (streaming) {
    // Update immediately without relying on the worker event loop
    if (worker)
      worker->configureImmediate(freqMHz, sampleRate);
    return;
  }
  streaming = true;

  thread = new QThread(this);
  worker = new Worker(); // no parent before move
  worker->moveToThread(thread);

  connect(worker, &Worker::newFFTData, this, &SDRReceiver::newFFTData,
          Qt::QueuedConnection);
  connect(thread, &QThread::started, [=]() {
    QMetaObject::invokeMethod(worker, "updateFftSize", Qt::QueuedConnection,
                              Q_ARG(int, currentFftSize));
    // Seed pending configuration for the worker loop to apply
    worker->configureImmediate(freqMHz, sampleRate);
    QMetaObject::invokeMethod(worker, "startWork", Qt::QueuedConnection);
  });
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);

  thread->start();
}

void SDRReceiver::stopStream() {
  if (!streaming)
    return;
  streaming = false;
  if (thread)
    thread->requestInterruption();
  if (worker) {
    QMetaObject::invokeMethod(worker, "endCapture", Qt::QueuedConnection);
    QMetaObject::invokeMethod(worker, "stopWork", Qt::QueuedConnection);
  }
  if (thread) {
    thread->quit();
    // Wait for the worker to finish and the event loop to quit
    if (!thread->wait(5000)) {
      // Last resort to avoid dangling running thread on app shutdown
      thread->terminate();
      thread->wait(1000);
    }
    delete thread;
    thread = nullptr;
    worker = nullptr;
  }
}

void SDRReceiver::setFftSize(int size) {
  int clamped = std::clamp(size, 512, 8192);
  currentFftSize = clamped;
  if (worker) {
    QMetaObject::invokeMethod(worker, "updateFftSize", Qt::QueuedConnection,
                              Q_ARG(int, clamped));
  }
}

void SDRReceiver::startCapture(const QString &filePath) {
  if (!streaming || !worker)
    return;
  QMetaObject::invokeMethod(worker, "beginCapture", Qt::QueuedConnection,
                            Q_ARG(QString, filePath));
}
void SDRReceiver::stopCapture() {
  if (!streaming || !worker)
    return;
  QMetaObject::invokeMethod(worker, "endCapture", Qt::QueuedConnection);
}

#include "SDRReceiver.moc"
