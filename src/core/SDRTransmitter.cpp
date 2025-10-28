#include "SDRTransmitter.h"
#include <QDebug>
#include <QCoreApplication>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.h>
#include <atomic>
#include <cmath>
#include <random>
#include <fftw3.h>

class SDRTransmitter::Worker : public QObject {
  Q_OBJECT
public:
  explicit Worker(QObject *parent = nullptr) : QObject(parent) {}
  ~Worker() { stopWork(); closeDevice(); }

  void configure(double freqMHz, double rate) {
    pendingFreqHz.store(freqMHz * 1e6, std::memory_order_release);
    pendingRate.store(rate, std::memory_order_release);
    reconfigureRequested.store(true, std::memory_order_release);
  }
  void setNoise(double intensity01, double halfSpanHz) {
    double amp = std::clamp(intensity01, 0.0, 1.0);
    desiredAmp.store(amp, std::memory_order_release);
    desiredHalfSpanHz.store(halfSpanHz, std::memory_order_release);
  }

public slots:
  void startWork() {
    running = true;
    if (!openDevice()) {
      qWarning() << "[TX] Failed to open HackRF device";
      running = false;
      return;
    }
    dev->activateStream(stream);
    qInfo() << "[TX] Stream activated";

    // Deterministic PRNG for predictable noise
    std::mt19937 rng(123456789u);
    std::normal_distribution<float> norm(0.0f, 1.0f);

    const int N = 4096;
    std::vector<std::complex<float>> buf(N);
    // FFTW buffers for precise band-limited noise synthesis
    fftwf_complex *freqBins = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * N);
    fftwf_complex *timeBuf = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * N);
    fftwf_plan ifftPlan = fftwf_plan_dft_1d(N, freqBins, timeBuf, FFTW_BACKWARD, FFTW_MEASURE);

    while (running.load(std::memory_order_acquire)) {
      // allow any queued calls (rare after we avoid queued stop, but safe)
      QCoreApplication::processEvents();
      if (QThread::currentThread()->isInterruptionRequested())
        running.store(false, std::memory_order_release);
      if (reconfigureRequested.load(std::memory_order_acquire)) {
        reconfigureRequested.store(false, std::memory_order_release);
        double f = pendingFreqHz.load(std::memory_order_acquire);
        double r = pendingRate.load(std::memory_order_acquire);
        applyTuning(f, r);
      }
      // Pull current parameters
      const double fs = rate;
      double halfSpan = desiredHalfSpanHz.load(std::memory_order_acquire);
      halfSpan = std::max(100.0, std::min(halfSpan, 0.45 * fs));
      // Keep HackRF analog baseband filter in-sync with requested span
      double bwWanted = std::max(2000.0, 2.0 * halfSpan);
      if (std::abs(bwWanted - lastBwHz) > 1.0) {
        try {
          dev->setBandwidth(SOAPY_SDR_TX, 0, bwWanted);
          lastBwHz = bwWanted;
          qInfo() << "[TX] Set baseband BW(Hz)=" << bwWanted;
        } catch (...) {
        }
      }
      float amp = static_cast<float>(std::min(0.95, (double)desiredAmp.load(std::memory_order_acquire)));
      // Frequency-domain band-limited noise (flat in-band, ~0 out-of-band)
      // Determine number of bins to fill around DC
      const double binHz = fs / N;
      int halfBins = std::clamp(int(std::floor(halfSpan / binHz)), 1, (N / 2) - 1);
      // Clear all bins
      for (int k = 0; k < N; ++k) {
        freqBins[k][0] = 0.0f;
        freqBins[k][1] = 0.0f;
      }
      // Fill positive low bins [1..halfBins]
      for (int k = 1; k <= halfBins; ++k) {
        freqBins[k][0] = norm(rng);
        freqBins[k][1] = norm(rng);
      }
      // Fill negative high bins [N-halfBins..N-1]
      for (int k = N - halfBins; k < N; ++k) {
        freqBins[k][0] = norm(rng);
        freqBins[k][1] = norm(rng);
      }
      // IFFT to time domain
      fftwf_execute(ifftPlan);
      const float invN = 1.0f / float(N);
      for (int i = 0; i < N; ++i) {
        // Scale and apply amplitude
        buf[i] = std::complex<float>(amp * invN * timeBuf[i][0],
                                     amp * invN * timeBuf[i][1]);
      }
      void *buffs[1];
      buffs[0] = buf.data();
      int flags = 0;
      long long timeNs = 0;
      int ret = dev->writeStream(stream, buffs, N, flags, timeNs, 200000);
      if (ret < 0) {
        qWarning() << "[TX] writeStream error:" << ret;
        // brief backoff to avoid busy loop if device unhappy
        QThread::msleep(2);
      }
    }

    dev->deactivateStream(stream);
    qInfo() << "[TX] Stream deactivated";
    // Free FFT resources
    fftwf_destroy_plan(ifftPlan);
    fftwf_free(freqBins);
    fftwf_free(timeBuf);
  }

  void stopWork() { running.store(false, std::memory_order_release); }

private:
  bool openDevice() {
    try {
      SoapySDR::Kwargs args;
      args["driver"] = "hackrf";
      dev = SoapySDR::Device::make(args);
      if (!dev)
        return false;
      // Initial defaults in case UI hasn't configured yet
      applyTuning(freqHz, rate);
      stream = dev->setupStream(SOAPY_SDR_TX, SOAPY_SDR_CF32);
      return stream != nullptr;
    } catch (...) {
      dev = nullptr;
      stream = nullptr;
      return false;
    }
  }

  void applyTuning(double fHz, double r) {
    if (!dev)
      return;
    rate = r;
    freqHz = fHz;
    try {
      dev->setSampleRate(SOAPY_SDR_TX, 0, rate);
    } catch (...) {
    }
    try {
      dev->setFrequency(SOAPY_SDR_TX, 0, freqHz);
    } catch (...) {
    }
    // Apply current bandwidth hint based on desired span, if any
    try {
      double bwWanted = std::max(2000.0, 2.0 * desiredHalfSpanHz.load(std::memory_order_acquire));
      dev->setBandwidth(SOAPY_SDR_TX, 0, bwWanted);
      lastBwHz = bwWanted;
    } catch (...) {
    }
    qInfo() << "[TX] Applied tuning freq(MHz)=" << freqHz / 1e6 << "rate="
            << rate;
  }

  void closeDevice() {
    if (dev) {
      if (stream) {
        try {
          dev->closeStream(stream);
        } catch (...) {
        }
        stream = nullptr;
      }
      SoapySDR::Device::unmake(dev);
      dev = nullptr;
    }
  }

  // device
  SoapySDR::Device *dev{nullptr};
  SoapySDR::Stream *stream{nullptr};
  // params
  std::atomic<bool> reconfigureRequested{false};
  std::atomic<double> pendingFreqHz{433.95e6};
  std::atomic<double> pendingRate{2.6e6};
  std::atomic<double> desiredAmp{0.5};
  std::atomic<double> desiredHalfSpanHz{100e3};
  double freqHz{433.95e6};
  double rate{2.6e6};
  std::atomic<bool> running{false};
  double lastBwHz{0.0};
};

SDRTransmitter::SDRTransmitter(QObject *parent) : QObject(parent) {}

SDRTransmitter::~SDRTransmitter() { stop(); }

void SDRTransmitter::start() {
  if (running)
    return;
  running = true;
  thread = new QThread;
  worker = new Worker;
  worker->moveToThread(thread);
  QObject::connect(thread, &QThread::started, [this]() {
    worker->configure(lastFreqMHz, lastSampleRate);
    worker->setNoise(lastIntensity, lastHalfSpanHz);
    QMetaObject::invokeMethod(worker, "startWork", Qt::QueuedConnection);
  });
  QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  thread->start();
}

void SDRTransmitter::stop() {
  if (!running)
    return;
  running = false;
  // Set stop flag immediately to avoid relying on queued delivery
  if (worker)
    worker->stopWork();
  if (thread) {
    thread->requestInterruption();
    thread->quit();
    // wait up to 3s for a clean stop
    if (!thread->wait(3000)) {
      qWarning() << "[TX] Thread did not quit cleanly, terminating";
      thread->terminate();
      thread->wait(1000);
    }
    delete thread;
    thread = nullptr;
    worker = nullptr;
  }
}

void SDRTransmitter::setFrequencyMHz(double freqMHz) {
  lastFreqMHz = freqMHz;
  if (worker)
    worker->configure(lastFreqMHz, lastSampleRate);
}

void SDRTransmitter::setSampleRate(double sampleRate) {
  lastSampleRate = sampleRate;
  if (worker)
    worker->configure(lastFreqMHz, lastSampleRate);
}

void SDRTransmitter::setNoiseIntensity(double intensity01) {
  lastIntensity = std::clamp(intensity01, 0.0, 1.0);
  if (worker)
    worker->setNoise(lastIntensity, lastHalfSpanHz);
}

void SDRTransmitter::setNoiseSpanHz(double halfSpanHz) {
  lastHalfSpanHz = std::max(100.0, halfSpanHz);
  if (worker)
    worker->setNoise(lastIntensity, lastHalfSpanHz);
}

#include "SDRTransmitter.moc"
