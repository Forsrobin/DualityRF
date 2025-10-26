
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
#include <math.h>

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
  void setGain(double gDb) {
    // Called in worker thread via queued connection
    pendingGain.store(gDb, std::memory_order_release);
    gainDb = gDb;
    if (dev) {
      // Ensure manual mode and apply immediately
      try {
        dev->setGainMode(SOAPY_SDR_RX, 0, false);
        // Try specific element names commonly used by RTL-SDR
        try { dev->setGain(SOAPY_SDR_RX, 0, "LNA", gainDb); } catch (...) {}
        try { dev->setGain(SOAPY_SDR_RX, 0, "TUNER", gainDb); } catch (...) {}
        // Fallback to aggregate
        try { dev->setGain(SOAPY_SDR_RX, 0, gainDb); } catch (...) {}
      } catch (...) {
      }
    }
    reconfigureRequested.store(true, std::memory_order_release);
  }

  void startWork() {
    running = true;
    activeFftSize =
        std::clamp(requestedFftSize.load(std::memory_order_acquire), 512, 8192);
    std::vector<std::complex<float>> buff(activeFftSize);
    std::vector<float> window(activeFftSize, 1.0f);
    std::vector<float> prevAmp(activeFftSize, 0.0f);
    float coherentGain = 1.0f; // sum(w)/N for amplitude normalization
    auto buildHann = [&](int N) {
      window.resize(N);
      double sumW = 0.0;
      for (int i = 0; i < N; ++i) {
        float w = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * float(i) / float(N - 1)));
        window[i] = w;
        sumW += w;
      }
      coherentGain = float(sumW / double(N));
      prevAmp.assign(N, 0.0f);
    };
    buildHann(activeFftSize);
    const float alpha = 0.4f; // smoothing factor, lower = more smoothing

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
        buildHann(activeFftSize);
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
        float w = window[i];
        in[i][0] = buff[i].real() * w;
        in[i][1] = buff[i].imag() * w;
      }
      fftwf_execute(plan);

      QVector<float> amps(activeFftSize);
      const float invN = 1.0f / float(activeFftSize);
      const float ampScale = invN / std::max(coherentGain, 1e-9f); // normalize FFT and window coherent gain
      for (int i = 0; i < activeFftSize; ++i) {
        float re = out[i][0] * ampScale;
        float im = out[i][1] * ampScale;
        float a = std::sqrt(re * re + im * im); // amplitude relative to full-scale
        // temporal smoothing in amplitude domain
        float s = alpha * a + (1.0f - alpha) * prevAmp[i];
        prevAmp[i] = s;
        // clamp to [0,1.5] to avoid crazy spikes from driver; UI will dB-map
        amps[i] = std::min(s, 1.5f);
      }
      // FFT shift: arrange bins as [-Fs/2 .. +Fs/2)
      QVector<float> ampsShift(activeFftSize);
      int half = activeFftSize / 2;
      for (int i = 0; i < activeFftSize; ++i)
        ampsShift[i] = amps[(i + half) % activeFftSize];
      emit newFFTData(ampsShift);

      // optional capture
      if (capturing && file.isOpen()) {
        file.write(reinterpret_cast<const char *>(buff.data()),
                   ret * sizeof(std::complex<float>));
      }

      if (reconfigureRequested.exchange(false)) {
        // Consume latest pending config
        double f = pendingFreqHz.load(std::memory_order_acquire);
        double r = pendingRate.load(std::memory_order_acquire);
        double g = pendingGain.load(std::memory_order_acquire);
        freqHz = f;
        rate = r;
        gainDb = g;
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
    dev->setGainMode(SOAPY_SDR_RX, 0, false);
    // Try specific and aggregate gain controls to cover driver differences
    try { dev->setGain(SOAPY_SDR_RX, 0, "LNA", gainDb); } catch (...) {}
    try { dev->setGain(SOAPY_SDR_RX, 0, "TUNER", gainDb); } catch (...) {}
    try { dev->setGain(SOAPY_SDR_RX, 0, gainDb); } catch (...) {}
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
  double gainDb{40.0};
  std::atomic<double> pendingFreqHz{433.81e6};
  std::atomic<double> pendingRate{2.6e6};
  std::atomic<double> pendingGain{40.0};

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
  lastFreqMHz = freqMHz;
  currentSampleRate = sampleRate;
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
    worker->setGain(currentGainDb);
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

void SDRReceiver::setGainDb(double gainDb) {
  currentGainDb = gainDb;
  if (worker) {
    QMetaObject::invokeMethod(worker, "setGain", Qt::QueuedConnection,
                              Q_ARG(double, gainDb));
  }
}

void SDRReceiver::setSampleRate(double sampleRate) {
  currentSampleRate = sampleRate;
  // If streaming, use existing immediate config pathway
  if (streaming && worker) {
    worker->configureImmediate(lastFreqMHz, sampleRate);
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
