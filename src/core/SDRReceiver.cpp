
#include "SDRReceiver.h"
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QMetaType>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <fftw3.h>
#include <math.h>
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
  void setGain(double gDb) {
    // Called in worker thread via queued connection
    pendingGain.store(gDb, std::memory_order_release);
    gainDb = gDb;
    if (dev) {
      // Ensure manual mode and apply immediately
      try {
        dev->setGainMode(SOAPY_SDR_RX, 0, false);
        // Disable any digital/tuner AGC settings some RTL stacks expose
        try {
          dev->writeSetting("rtl_agc", "false");
        } catch (...) {
        }
        try {
          dev->writeSetting("tuner_agc", "false");
        } catch (...) {
        }
        // Try specific element names commonly used by RTL-SDR
        try {
          dev->setGain(SOAPY_SDR_RX, 0, "LNA", gainDb);
        } catch (...) {
        }
        try {
          dev->setGain(SOAPY_SDR_RX, 0, "TUNER", gainDb);
        } catch (...) {
        }
        // Fallback to aggregate
        try {
          dev->setGain(SOAPY_SDR_RX, 0, gainDb);
        } catch (...) {
        }
      } catch (...) {
      }
    }
    reconfigureRequested.store(true, std::memory_order_release);
  }

  void setThresholdDb(double db) {
    triggerThresholdDb.store(db, std::memory_order_release);
    qInfo() << "[RX] Set trigger threshold (dB)=" << db;
  }

  void setCaptureSpan(double halfSpanHz) {
    if (halfSpanHz < 0.0)
      halfSpanHz = 0.0;
    captureSpanHalfHz.store(halfSpanHz, std::memory_order_release);
    qInfo() << "[RX] Set capture span half-width (Hz)=" << halfSpanHz;
  }

  void setDetectorModeSlot(int mode) {
    int m = (mode == 1) ? 1 : 0;
    detectorMode.store(m, std::memory_order_release);
    qInfo() << "[RX] Set detector mode ->" << (m == 0 ? "Averaged" : "Peak");
  }

  void armCapture(double preSec, double postSec) {
    qInfo() << "[RX] Arm capture pre(s)=" << preSec << "post(s)=" << postSec
            << "rate=" << rate << "freq(MHz)=" << freqHz / 1e6;
    armed.store(true, std::memory_order_release);
    inCapture.store(false, std::memory_order_release);
    preSeconds = std::max(0.0, preSec);
    postSeconds = std::max(0.0, postSec);
    belowSamples = 0;
    totalSamplesSinceArm = 0;
    preHead = 0;
    centerAvgLin = 0.0;
    aboveStreakSamples = 0;
    // begin visible on-disk spooling so user sees a file immediately
    // we will delete this temporary once the trimmed capture is written
    if (spoolFile.isOpen())
      spoolFile.close();
    QDir().mkpath("captures");
    armStartTime = QDateTime::currentDateTimeUtc();
    QString ts = armStartTime.toString("yyyyMMdd_HHmmss");
    double rxMHz = freqHz / 1e6;
    spoolPath = QString("captures/in_progress_%1_RX%2.cf32.part")
                    .arg(ts)
                    .arg(rxMHz, 0, 'f', 3);
    spoolFile.setFileName(spoolPath);
    if (!spoolFile.open(QIODevice::WriteOnly)) {
      // if we cannot open, just proceed without spooling
      spoolPath.clear();
    }
    if (!spoolPath.isEmpty())
      qInfo() << "[RX] Spooling to" << spoolPath;
    // configure prebuffer capacity based on current sample rate
    preBufferCap = static_cast<uint64_t>(std::llround(rate * preSeconds));
    preBuffer.clear();
    if (preBufferCap > 0) {
      preBuffer.resize(static_cast<size_t>(preBufferCap));
      preHead = 0;
      preFilled = 0;
    }
  }

  void cancelCapture() {
    qInfo() << "[RX] Cancel capture";
    armed.store(false, std::memory_order_release);
    inCapture.store(false, std::memory_order_release);
    belowSamples = 0;
    totalSamplesSinceArm = 0;
    preHead = 0;
    preFilled = 0;
    preBuffer.clear();
    captureBuffer.clear();
    centerAvgLin = 0.0;
    aboveStreakSamples = 0;
    if (spoolFile.isOpen()) {
      spoolFile.close();
      if (!spoolPath.isEmpty())
        QFile::remove(spoolPath);
      spoolPath.clear();
    }
  }

  void startWork() {
    running = true;
    qInfo() << "[RX] Worker thread start";
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
        float w =
            0.5f *
            (1.0f - std::cos(2.0f * float(M_PI) * float(i) / float(N - 1)));
        window[i] = w;
        sumW += w;
      }
      coherentGain = float(sumW / double(N));
      prevAmp.assign(N, 0.0f);
    };
    buildHann(activeFftSize);
    const float alpha = 0.4f; // smoothing factor, lower = more smoothing

    while (running) {
      // Allow queued invocations (arm/cancel/threshold/span/mode/etc.)
      // to be processed while this long-running loop is active.
      QCoreApplication::processEvents();
      if (QThread::currentThread()->isInterruptionRequested())
        running = false;
      if (!dev)
        openDevice(); // lazy open
      if (!dev) {
        QThread::msleep(200);
        continue;
      }

      int desired = std::clamp(requestedFftSize.load(std::memory_order_acquire),
                               512, 8192);
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
      const float ampScale =
          invN / std::max(coherentGain,
                          1e-9f); // normalize FFT and window coherent gain
      for (int i = 0; i < activeFftSize; ++i) {
        float re = out[i][0] * ampScale;
        float im = out[i][1] * ampScale;
        float a =
            std::sqrt(re * re + im * im); // amplitude relative to full-scale
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

      // Triggered capture logic
      if (armed.load(std::memory_order_acquire)) {
        // Continuously spool raw samples to a temporary file so the user
        // sees a file immediately while armed.
        if (spoolFile.isOpen()) {
          spoolFile.write(reinterpret_cast<const char *>(buff.data()),
                          ret * sizeof(std::complex<float>));
        }
        // 1) maintain 1s prebuffer ring
        if (preBufferCap > 0) {
          for (int i = 0; i < ret; ++i) {
            if (preBuffer.empty())
              break; // safety
            preBuffer[static_cast<size_t>(preHead)] = buff[i];
            preHead = (preHead + 1) % preBufferCap;
            if (preFilled < preBufferCap)
              ++preFilled;
          }
        }
        totalSamplesSinceArm += static_cast<uint64_t>(ret);

        // 2) detect activity near RX center within ~±100 kHz (or at least ±2
        // bins)
        const int half = activeFftSize / 2;
        double binHz =
            (activeFftSize > 0) ? (rate / double(activeFftSize)) : 0.0;
        int winBins = 2;
        if (binHz > 0.0) {
          double spanHz = captureSpanHalfHz.load(std::memory_order_acquire);
          if (spanHz <= 0.0)
            spanHz = 100000.0; // default ±100 kHz
          winBins = std::max(2, int(std::ceil(spanHz / binHz)));
          winBins = std::min(winBins, half - 1);
        }
        float centerMax = 0.0f;
        int startBin = std::max(0, half - winBins);
        int endBin = std::min(activeFftSize - 1, half + winBins);
        for (int idx = startBin; idx <= endBin; ++idx)
          centerMax = std::max(centerMax, ampsShift[idx]);
        const float eps = 1e-6f;
        // Choose detector: averaged vs peak
        double centerDb = 0.0;
        if (detectorMode.load(std::memory_order_acquire) == 0) {
          // Averaged detector with ~avgTauSeconds time constant
          double dtSec = (rate > 0.0) ? (double(ret) / rate) : 0.0;
          double alphaAvg = 0.0;
          if (dtSec > 0.0 && avgTauSeconds > 0.0)
            alphaAvg = 1.0 - std::exp(-dtSec / avgTauSeconds);
          centerAvgLin =
              (1.0 - alphaAvg) * centerAvgLin + alphaAvg * double(centerMax);
          centerDb = 20.0 * std::log10(std::max(centerAvgLin, double(eps)));
        } else {
          // Peak detector (instantaneous)
          centerDb =
              20.0 * std::log10(std::max(double(centerMax), double(eps)));
        }
        const double thrDb = triggerThresholdDb.load(std::memory_order_acquire);
        const bool aboveAvg = (centerDb >= thrDb);
        if (aboveAvg != lastAbove) {
          lastAbove = aboveAvg;
          qInfo() << "[RX] Trigger" << (aboveAvg ? "ABOVE" : "below")
                  << "center(dB)=" << centerDb << "thr(dB)=" << thrDb;
        }

        // accumulate above time for dwell requirement
        if (aboveAvg)
          aboveStreakSamples += static_cast<uint64_t>(ret);
        else
          aboveStreakSamples = 0;

        // notify trigger status based on averaged value
        emit triggerStatus(true, inCapture.load(std::memory_order_acquire),
                           centerDb, thrDb, aboveAvg);
        // periodic debug log while armed
        logSamplesAccum += static_cast<uint64_t>(ret);
        const uint64_t logEvery =
            static_cast<uint64_t>(std::llround(rate * 0.5));
        if (logSamplesAccum >= std::max<uint64_t>(logEvery, 1)) {
          qInfo() << "[RX] Armed center(dB)=" << centerDb << "thr(dB)=" << thrDb
                  << "above=" << aboveAvg
                  << "capturing=" << inCapture.load(std::memory_order_acquire);
          logSamplesAccum = 0;
        }

        if (!inCapture.load(std::memory_order_acquire)) {
          uint64_t needAbove =
              static_cast<uint64_t>(std::llround(rate * dwellSeconds));
          if (detectorMode.load(std::memory_order_acquire) == 1) {
            // Peak detector: require just one block above
            needAbove = std::max<uint64_t>(ret, 1);
          }
          if (aboveStreakSamples >= needAbove) {
            // Start capture: copy prebuffer content in chronological order
            inCapture.store(true, std::memory_order_release);
            captureBuffer.clear();
            qInfo() << "[RX] Capture START (preFilled=" << preFilled
                    << ", fftSize=" << activeFftSize << ")";
            if (preFilled > 0 && !preBuffer.empty()) {
              uint64_t count = preFilled;
              uint64_t head =
                  preHead; // points to oldest position to be overwritten next
              // oldest sample is at head when full; when partially filled,
              // oldest at 0
              if (preFilled == preBufferCap) {
                // full ring: start from head to end, then 0 to head-1
                for (uint64_t i = 0; i < preBufferCap; ++i) {
                  uint64_t idx = (head + i) % preBufferCap;
                  captureBuffer.push_back(preBuffer[static_cast<size_t>(idx)]);
                }
              } else {
                // not full yet: take 0..preFilled-1
                captureBuffer.insert(captureBuffer.end(), preBuffer.begin(),
                                     preBuffer.begin() +
                                         static_cast<long>(preFilled));
              }
            }
            // include current chunk
            captureBuffer.insert(captureBuffer.end(), buff.begin(),
                                 buff.begin() + ret);
            belowSamples = 0;
          }
        } else {
          // already capturing, keep appending
          captureBuffer.insert(captureBuffer.end(), buff.begin(),
                               buff.begin() + ret);
          if (aboveAvg) {
            belowSamples = 0;
          } else {
            belowSamples += static_cast<uint64_t>(ret);
            const uint64_t needPost =
                static_cast<uint64_t>(std::llround(rate * postSeconds));
            if (belowSamples >= needPost) {
              // finalize: write buffer to file
              QString outPath = makeCapturePath();
              if (!outPath.isEmpty()) {
                QFile out(outPath);
                if (out.open(QIODevice::WriteOnly)) {
                  if (!captureBuffer.empty()) {
                    out.write(
                        reinterpret_cast<const char *>(captureBuffer.data()),
                        qint64(captureBuffer.size() *
                               sizeof(std::complex<float>)));
                  }
                  out.close();
                }
              }
              qInfo() << "[RX] Capture COMPLETE ->" << outPath
                      << "samples=" << captureBuffer.size();
              // cleanup spooling temp
              if (spoolFile.isOpen())
                spoolFile.close();
              if (!spoolPath.isEmpty()) {
                QFile::remove(spoolPath);
                spoolPath.clear();
              }
              // reset
              armed.store(false, std::memory_order_release);
              inCapture.store(false, std::memory_order_release);
              belowSamples = 0;
              totalSamplesSinceArm = 0;
              preHead = 0;
              preFilled = 0;
              preBuffer.clear();
              captureBuffer.clear();
              centerAvgLin = 0.0;
              aboveStreakSamples = 0;
              emit captureCompleted(outPath);
            }
          }
        }
      }

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
    qInfo() << "[RX] Manual capture BEGIN ->" << path;
  }
  void endCapture() {
    capturing = false;
    if (file.isOpen())
      file.close();
    qInfo() << "[RX] Manual capture END";
  }
  void updateFftSize(int size) {
    int clamped = std::clamp(size, 512, 8192);
    requestedFftSize.store(clamped, std::memory_order_release);
  }
  void setCaptureSpanHzSlot(double halfSpanHz) { setCaptureSpan(halfSpanHz); }

  // Thread-safe: may be called from any thread
  void configureImmediate(double freqMHz, double sampleRate) {
    pendingFreqHz.store(freqMHz * 1e6, std::memory_order_release);
    pendingRate.store(sampleRate, std::memory_order_release);
    reconfigureRequested.store(true, std::memory_order_release);
  }

private:
  QString makeCapturePath() {
    QDir().mkpath("captures");
    QString ts = armStartTime.toString("yyyyMMdd_HHmmss");
    double rxMHz = freqHz / 1e6;
    double thr = triggerThresholdDb.load(std::memory_order_acquire);
    return QString("captures/%1_RX%2_thr%3.cf32")
        .arg(ts)
        .arg(rxMHz, 0, 'f', 3)
        .arg(thr, 0, 'f', 0);
  }
  void openDevice() {
    try {
      SoapySDR::Kwargs args;
      args["driver"] = "rtlsdr";
      dev = SoapySDR::Device::make(args);
      stream = dev->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
      applyTuning();
      dev->activateStream(stream);
      qInfo() << "[RX] Device opened + stream activated";
    } catch (...) {
      qWarning() << "[RX] Failed to open RTL-SDR device";
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
    try {
      dev->writeSetting("rtl_agc", "false");
    } catch (...) {
    }
    try {
      dev->writeSetting("tuner_agc", "false");
    } catch (...) {
    }
    // Try specific and aggregate gain controls to cover driver differences
    try {
      dev->setGain(SOAPY_SDR_RX, 0, "LNA", gainDb);
    } catch (...) {
    }
    try {
      dev->setGain(SOAPY_SDR_RX, 0, "TUNER", gainDb);
    } catch (...) {
    }
    try {
      dev->setGain(SOAPY_SDR_RX, 0, gainDb);
    } catch (...) {
    }
    qInfo() << "[RX] Applied tuning" << "freq(MHz)=" << freqHz / 1e6
            << "rate=" << rate << "gain(dB)=" << gainDb;
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
  std::atomic<bool> armed{false};
  std::atomic<bool> inCapture{false};
  std::atomic<double> triggerThresholdDb{-30.0};
  std::atomic<double> captureSpanHalfHz{100000.0};
  std::atomic<int> detectorMode{0}; // 0 averaged, 1 peak
  double preSeconds{0.2};
  double postSeconds{0.2};
  double dwellSeconds{0.02};
  // Slightly quicker response to short packets
  // (can be adjusted via code if needed)
  // NOTE: this is the default value; logic uses this to compute required
  // samples above threshold before starting capture.

  double avgTauSeconds{0.20};
  uint64_t belowSamples{0};
  uint64_t totalSamplesSinceArm{0};
  uint64_t logSamplesAccum{0};
  double centerAvgLin{0.0};
  uint64_t aboveStreakSamples{0};

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
  // live spooling while armed (for user feedback)
  QFile spoolFile;
  QString spoolPath;
  // Triggered capture buffers
  std::vector<std::complex<float>> preBuffer; // ring buffer storage
  uint64_t preBufferCap{0};
  uint64_t preFilled{0};
  uint64_t preHead{0}; // next position to overwrite
  std::vector<std::complex<float>> captureBuffer;
  QDateTime armStartTime;
  bool lastAbove{false};

signals:
  void newFFTData(QVector<float> data);
  void captureCompleted(QString filePath);
  void triggerStatus(bool armed, bool capturing, double centerDb,
                     double thresholdDb, bool above);
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
  connect(worker, &Worker::captureCompleted, this,
          &SDRReceiver::captureCompleted, Qt::QueuedConnection);
  connect(worker, &Worker::triggerStatus, this, &SDRReceiver::triggerStatus,
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

void SDRReceiver::setTriggerThresholdDb(double thresholdDb) {
  if (worker) {
    QMetaObject::invokeMethod(worker, "setThresholdDb", Qt::QueuedConnection,
                              Q_ARG(double, thresholdDb));
  }
}

void SDRReceiver::setCaptureSpanHz(double halfSpanHz) {
  if (worker) {
    QMetaObject::invokeMethod(worker, "setCaptureSpanHzSlot",
                              Qt::QueuedConnection, Q_ARG(double, halfSpanHz));
  }
}

void SDRReceiver::setDetectorMode(int mode) {
  if (worker) {
    QMetaObject::invokeMethod(worker, "setDetectorModeSlot",
                              Qt::QueuedConnection, Q_ARG(int, mode));
  }
}

void SDRReceiver::armTriggeredCapture(double preSeconds, double postSeconds) {
  if (!streaming || !worker)
    return;
  QMetaObject::invokeMethod(worker, "armCapture", Qt::QueuedConnection,
                            Q_ARG(double, preSeconds),
                            Q_ARG(double, postSeconds));
}

void SDRReceiver::cancelTriggeredCapture() {
  if (!streaming || !worker)
    return;
  QMetaObject::invokeMethod(worker, "cancelCapture", Qt::QueuedConnection);
}

#include "SDRReceiver.moc"
