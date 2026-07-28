#include "pipeline/RepeaterPipeline.h"

#include "dsp/BandTrimmer.h"
#include "dsp/SpectrumProcessor.h"
#include "sdr/IReceiver.h"
#include "sdr/ITransmitter.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace duality {

namespace {
constexpr std::size_t kChunkSamples = 16384;
// Samples kept from just before onset so a detection that lags the real start
// does not clip the head of the burst (~50 ms of stream).
constexpr double kPreRollSec = 0.05;
// The envelope must fall this far below the trigger level before a burst is
// considered over, giving the detector hysteresis so it does not chatter.
constexpr float kHysteresisDb = 3.0f;
} // namespace

RepeaterPipeline::RepeaterPipeline(SpectrumProcessor *spectrum, QObject *parent)
    : QObject(parent), m_spectrum(spectrum) {}

RepeaterPipeline::~RepeaterPipeline() { stop(); }

bool RepeaterPipeline::start(std::shared_ptr<ISDRDevice> rxDevice,
                             std::shared_ptr<ISDRDevice> txDevice,
                             const Config &config) {
  stop();

  IReceiver *rx = rxDevice ? rxDevice->receiver() : nullptr;
  if (!rx) {
    emit errorOccurred(tr("Selected device cannot receive"));
    return false;
  }
  ITransmitter *tx = txDevice ? txDevice->transmitter() : nullptr;
  if (!tx) {
    emit errorOccurred(tr("Selected device cannot transmit"));
    return false;
  }
  if (!rx->configure(config.rxParams)) {
    emit errorOccurred(tr("Receiver rejected the stream parameters"));
    return false;
  }

  m_rxDevice = std::move(rxDevice);
  m_txDevice = std::move(txDevice);
  m_config = config;

  SpectrumProcessor::Config specCfg;
  m_spectrum->start(specCfg);

  m_worker = std::jthread([this](std::stop_token st) { workerLoop(st); });
  m_running = true;
  emit started();
  return true;
}

void RepeaterPipeline::stop() {
  // Join the worker first so it can no longer launch a new transmit thread,
  // then tear down any in-flight retransmission.
  if (m_worker.joinable()) {
    m_worker.request_stop();
    m_worker.join();
  }
  if (m_txThread.joinable()) {
    m_txThread.request_stop();
    m_txThread.join();
  }
  m_txActive = false;
  if (m_spectrum->running())
    m_spectrum->stop();
  m_rxDevice.reset();
  m_txDevice.reset();
  m_running = false;
}

void RepeaterPipeline::workerLoop(std::stop_token st) {
  using clock = std::chrono::steady_clock;

  IReceiver *rx = m_rxDevice->receiver();

  const auto fail = [this](const QString &msg) {
    QMetaObject::invokeMethod(
        this,
        [this, msg] {
          m_running = false;
          emit errorOccurred(msg);
        },
        Qt::QueuedConnection);
  };
  const auto emitPhase = [this](void (RepeaterPipeline::*sig)()) {
    QMetaObject::invokeMethod(this, [this, sig] { (this->*sig)(); },
                              Qt::QueuedConnection);
  };

  // (Re)tune and start the receive stream. Called after every retransmission
  // because the transmit stage retunes a shared full-duplex device.
  const auto startRx = [&]() -> bool {
    return rx->configure(m_config.rxParams) && rx->start();
  };
  if (!startRx()) {
    fail(tr("Failed to start the receive stream"));
    return;
  }
  emitPhase(&RepeaterPipeline::listening);

  StreamParams txParams = m_config.rxParams;
  txParams.frequencyHz = m_config.txFrequencyHz;
  txParams.gainDb = m_config.txGainDb;

  const double rate = m_config.rxParams.sampleRateHz;
  const auto maxSamples =
      static_cast<std::size_t>(std::max(1.0, m_config.maxCaptureSec * rate));
  const auto preRollCap =
      static_cast<std::size_t>(std::max(0.0, kPreRollSec * rate));
  const float onDb = m_config.thresholdDb;
  const float offDb = m_config.thresholdDb - kHysteresisDb;

  // Retransmit one captured burst through the transmitter, stopping early if
  // stopReq() becomes true. Emits repeated() on success; returns false only if
  // the transmit stream could not be opened.
  const auto transmit = [this, txParams, rate](const std::vector<Complex> &buf,
                                               auto stopReq) -> bool {
    ITransmitter *tx = m_txDevice->transmitter();
    if (!(tx->configure(txParams) && tx->start())) {
      tx->stop();
      return false;
    }
    std::size_t offset = 0;
    while (offset < buf.size() && !stopReq())
      offset += tx->write(
          std::span<const Complex>(buf.data() + offset, buf.size() - offset));
    tx->stop();
    const qint64 n = static_cast<qint64>(buf.size());
    const double secs = rate > 0.0 ? n / rate : 0.0;
    QMetaObject::invokeMethod(
        this, [this, n, secs] { emit repeated(n, secs); },
        Qt::QueuedConnection);
    return true;
  };

  std::vector<Complex> chunk(kChunkSamples);
  std::vector<Complex> capture;
  std::vector<Complex> preRoll;
  bool capturing = false;
  clock::time_point lastSignal{};
  clock::time_point resumeDetectAt{};

  while (!st.stop_requested()) {
    const std::size_t got = rx->read(std::span(chunk.data(), chunk.size()));
    if (got == 0)
      continue; // timeout/overflow; re-check the stop token

    // Feed the live spectrum and compute the chunk's mean power in dBFS.
    m_spectrum->buffer()->write(std::span(chunk.data(), got));
    double sumSq = 0.0;
    for (std::size_t i = 0; i < got; ++i)
      sumSq += std::norm(chunk[i]);
    const auto powerDb =
        static_cast<float>(10.0 * std::log10(sumSq / got + 1e-12));

    // Keep the viz live while a burst is being replayed on the side thread, but
    // suppress detection (and hold off briefly afterwards) so the repeater never
    // captures and re-repeats its own replay.
    if (m_txActive.load()) {
      resumeDetectAt = clock::now() + m_config.hangTime;
      continue;
    }
    if (clock::now() < resumeDetectAt)
      continue;

    if (!capturing) {
      // Keep a rolling tail so a burst can be seeded with pre-roll.
      preRoll.insert(preRoll.end(), chunk.begin(),
                     chunk.begin() + static_cast<std::ptrdiff_t>(got));
      if (preRoll.size() > preRollCap)
        preRoll.erase(preRoll.begin(),
                      preRoll.end() - static_cast<std::ptrdiff_t>(preRollCap));
      if (powerDb >= onDb) {
        capturing = true;
        capture = std::move(preRoll); // already includes this chunk
        preRoll.clear();
        lastSignal = clock::now();
        emitPhase(&RepeaterPipeline::capturing);
      }
      continue;
    }

    // Capturing: append until the signal has been gone for the hang time or the
    // buffer hits its safety cap.
    capture.insert(capture.end(), chunk.begin(),
                   chunk.begin() + static_cast<std::ptrdiff_t>(got));
    if (powerDb >= offDb)
      lastSignal = clock::now();
    const bool hangExpired =
        clock::now() - lastSignal > m_config.hangTime;
    const bool full = capture.size() >= maxSamples;
    if (!hangExpired && !full)
      continue;

    // Burst over: retransmit it once.
    capturing = false;
    emitPhase(&RepeaterPipeline::transmitting);

    // Band-limit the burst to the capture range before replay so only that
    // slice of the band is rebroadcast (the trimmer keeps the sample rate, so
    // the transmit hardware still streams it).
    std::vector<Complex> burst;
    if (m_config.captureRangeHz > 0.0) {
      const BandTrimmer trimmer(rate, m_config.captureRangeHz);
      burst = trimmer.process(capture);
    } else {
      burst = std::move(capture);
    }
    capture.clear(); // reassigned when the next burst opens

    // Replay on a side thread so this loop keeps receiving — the spectrum and
    // waterfall show the replayed signal live. This needs a radio that can
    // receive and transmit at once; on a half-duplex device transmit() fails
    // and we report it (the receiver keeps running regardless).
    if (m_txThread.joinable())
      m_txThread.join(); // the previous burst has already finished
    m_txActive = true;
    m_txThread = std::jthread(
        [this, buf = std::move(burst), transmit](std::stop_token tst) {
          if (!transmit(buf, [&tst] { return tst.stop_requested(); }))
            QMetaObject::invokeMethod(
                this,
                [this] {
                  emit errorOccurred(
                      tr("Transmit failed — a half-duplex radio cannot receive "
                         "and transmit at once; use separate RX and TX "
                         "devices."));
                },
                Qt::QueuedConnection);
          m_txActive = false;
          QMetaObject::invokeMethod(
              this, [this] { emit listening(); }, Qt::QueuedConnection);
        });
    resumeDetectAt = clock::now() + m_config.hangTime;
  }

  rx->stop();
  QMetaObject::invokeMethod(
      this,
      [this] {
        m_running = false;
        emit finished();
      },
      Qt::QueuedConnection);
}

} // namespace duality
