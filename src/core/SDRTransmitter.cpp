#include "SDRTransmitter.h"
#include <QCoreApplication>
#include <QDebug>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.h>
#include <atomic>
#include <cmath>
#include <fftw3.h>
#include <random>

class SDRTransmitter::Worker : public QObject {
  Q_OBJECT
public:
  explicit Worker(QObject *parent = nullptr) : QObject(parent) {}
  ~Worker() {
    stopWork();
    closeDevice();
  }

  void configure(double freqMHz, double rate) {
    pendingFreqHz.store(freqMHz * 1e6, std::memory_order_release);
    pendingRate.store(rate, std::memory_order_release);
    reconfigureRequested.store(true, std::memory_order_release);
  }
  void setNoise(double intensity01, double halfSpanHz) {
    Q_UNUSED(intensity01);
    desiredHalfSpanHz.store(halfSpanHz, std::memory_order_release);
  }
  void setLevelDbfs(double dbfs) {
    targetDbfs.store(dbfs, std::memory_order_release);
  }
  void setTxVga(double gainDb) {
    double g = std::clamp(gainDb, 0.0, 47.0);
    requestedTxVga.store(g, std::memory_order_release);
  }

public slots:
  void startWork() {
    running = true;
    if (!openDevice()) {
      qWarning() << "[TX] Failed to open HackRF device";
      running = false;
      return;
    }
    // Stream already activated in openDevice()
    qInfo() << "[TX] Stream activated";

    // Deterministic PRNG for predictable noise
    std::mt19937 rng(123456789u);
    std::normal_distribution<float> norm(0.0f, 1.0f);
    const int N = 4096;
    std::vector<std::complex<float>> buf(N);
    // Precomputed cyclic waveform of band-limited complex noise
    std::vector<std::complex<float>> wave;
    int waveN = 0;
    size_t wavePos = 0;
    double waveFs = 0.0;
    double waveHalfSpan = 0.0;
    auto rebuildWave = [&](double fs, double halfSpan) {
      // Build a large block of band-limited noise in frequency domain and IFFT
      int Nwave = 1 << 18; // 262144 samples ~0.1s at 2.6 Msps
      if (waveN != Nwave)
        wave.resize(Nwave);
      std::vector<fftwf_complex> freqBins(Nwave);
      std::vector<fftwf_complex> timeBuf(Nwave);
      fftwf_plan plan = fftwf_plan_dft_1d(Nwave, freqBins.data(), timeBuf.data(),
                                          FFTW_BACKWARD, FFTW_ESTIMATE);
      // Clear bins
      for (int k = 0; k < Nwave; ++k) {
        freqBins[k][0] = 0.0f;
        freqBins[k][1] = 0.0f;
      }
      double binHz = fs / double(Nwave);
      int halfBins = std::clamp(int(std::floor(halfSpan / binHz)), 1, (Nwave / 2) - 1);
      int notchBins = std::max(0, int(std::round(1500.0 / binHz))); // ~1.5 kHz notch
      // Fill positive [1..halfBins]
      for (int k = 1; k <= halfBins; ++k) {
        if (k <= notchBins) continue;
        freqBins[k][0] = norm(rng);
        freqBins[k][1] = norm(rng);
      }
      // Fill negative [Nwave-halfBins..Nwave-1]
      for (int k = Nwave - halfBins; k < Nwave; ++k) {
        int dist = k - (Nwave - halfBins) + 1; // 1..halfBins
        if (dist <= notchBins) continue;
        freqBins[k][0] = norm(rng);
        freqBins[k][1] = norm(rng);
      }
      fftwf_execute(plan);
      fftwf_destroy_plan(plan);
      // Normalize RMS magnitude to 1.0 and copy to wave
      double acc = 0.0;
      const float invNwave = 1.0f / float(Nwave);
      for (int i = 0; i < Nwave; ++i) {
        float re = invNwave * timeBuf[i][0];
        float im = invNwave * timeBuf[i][1];
        acc += double(re) * double(re) + double(im) * double(im);
        wave[i] = std::complex<float>(re, im);
      }
      double rms = std::sqrt(acc / double(Nwave));
      float scale = (rms > 1e-12) ? float(1.0 / rms) : 1.0f;
      for (int i = 0; i < Nwave; ++i)
        wave[i] *= scale;
      waveN = Nwave;
      wavePos = 0;
      waveFs = fs;
      waveHalfSpan = halfSpan;
      qInfo() << "[TX] Wave rebuilt N=" << Nwave << "halfSpanHz=" << halfSpan;
    };

    while (running.load(std::memory_order_acquire)) {
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
      // Rebuild waveform if needed
      if (wave.empty() || std::abs(waveHalfSpan - halfSpan) > 500.0 ||
          std::abs(waveFs - fs) > 1.0) {
        rebuildWave(fs, halfSpan);
      }
      // Desired magnitude RMS from dBFS
      double dbfs = targetDbfs.load(std::memory_order_acquire);
      dbfs = std::clamp(dbfs, -80.0, 0.0);
      float targetMagRms = std::pow(10.0, dbfs / 20.0);
      // Copy from cyclic waveform and scale to target RMS
      for (int i = 0; i < N; ++i) {
        const std::complex<float> s = wave[wavePos];
        wavePos = (wavePos + 1) % size_t(waveN);
        buf[i] = targetMagRms * s;
      }
      // no additional multi-tone mix; FIR-shaped Gaussian noise is flat
      int flags = 0;
      long long timeNs = 0;
      int written = 0;
      while (written < N && running.load(std::memory_order_acquire)) {
        void *buffs[1];
        buffs[0] = (void *)(buf.data() + written);
        int ret = dev->writeStream(stream, buffs, N - written, flags, timeNs,
                                   200000);
        if (ret > 0) {
          written += ret;
          continue;
        }
        // transient error/timeout; yield briefly and retry to keep continuity
        QThread::usleep(500);
      }
    }

    dev->deactivateStream(stream);
    qInfo() << "[TX] Stream deactivated";
    // no FFT resources to free
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
      // Set TX gains: enable PA/AMP and set a sane VGA level so
      // digitally generated noise is visible above LO leakage.
      try {
        dev->setGain(SOAPY_SDR_TX, 0, "AMP", 1.0); // enable PA (boolean on most hackrf)
      } catch (...) {
      }
      try {
        dev->setGain(SOAPY_SDR_TX, 0, "PA", 1.0); // alternative name on some builds
      } catch (...) {
      }
      try { // initial VGA from requested value
        dev->setGain(SOAPY_SDR_TX, 0, "VGA",
                     requestedTxVga.load(std::memory_order_acquire));
        lastTxVga = requestedTxVga.load(std::memory_order_acquire);
      } catch (...) {
      }
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
      double bwWanted = std::max(
          2000.0, 2.0 * desiredHalfSpanHz.load(std::memory_order_acquire));
      dev->setBandwidth(SOAPY_SDR_TX, 0, bwWanted);
      lastBwHz = bwWanted;
    } catch (...) {
    }
    qInfo() << "[TX] Applied tuning freq(MHz)=" << freqHz / 1e6
            << "rate=" << rate;
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
  std::atomic<double> targetDbfs{-30.0};
  std::atomic<double> requestedTxVga{25.0};
  double freqHz{434.20e6};
  double rate{2.6e6};
  std::atomic<bool> running{false};
  double lastBwHz{0.0};
  double lastTxVga{-1.0};
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
    worker->setLevelDbfs(lastNoiseDbfs);
    worker->setTxVga(lastTxGainDb);
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

void SDRTransmitter::setNoiseLevelDb(double dbfs) {
  lastNoiseDbfs = dbfs;
  if (worker)
    QMetaObject::invokeMethod(worker, "setLevelDbfs", Qt::QueuedConnection,
                              Q_ARG(double, dbfs));
}

void SDRTransmitter::setTxGainDb(double gainDb) {
  lastTxGainDb = std::clamp(gainDb, 0.0, 47.0);
  if (worker)
    QMetaObject::invokeMethod(worker, "setTxVga", Qt::QueuedConnection,
                              Q_ARG(double, lastTxGainDb));
}

#include "SDRTransmitter.moc"
