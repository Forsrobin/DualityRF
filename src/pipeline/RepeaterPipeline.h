#pragma once

#include "core/Types.h"
#include "sdr/ISDRDevice.h"

#include <QObject>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace duality {

class SpectrumProcessor;

// In-memory store-and-forward repeater. One worker thread receives on a
// channel, watches the envelope for a transmission (simple power detector with
// hang time), buffers the raw IQ of that burst in memory, then — as soon as the
// signal drops — retransmits the captured samples once through the transmitter,
// optionally shifted by a carrier offset. The receiver keeps running while the
// retransmission plays out on a side thread, so the live spectrum shows the
// replayed signal (detection is suppressed during the replay so it never
// repeats its own output). This needs a radio that can receive and transmit at
// once — a full-duplex device, or a separate receiver and transmitter; on a
// strictly half-duplex device the retransmit fails and errorOccurred() fires.
class RepeaterPipeline : public QObject {
    Q_OBJECT
public:
    struct Config {
        StreamParams rxParams;      // receive tuning (freq, rate, gain, bw)
        double txFrequencyHz = 0.0; // retransmit carrier (rx freq + offset)
        double txGainDb = 0.0;
        // Trigger level for the time-domain power detector, in dBFS (mean
        // |x|^2 of a chunk). A burst starts when the level crosses this and
        // ends after it stays below for the hang time.
        float thresholdDb = -50.0f;
        // Half-width (± Hz) the captured burst is band-limited to before replay
        // (0 = keep the full received span).
        double captureRangeHz = 0.0;
        // How long the signal must stay gone before the burst is considered
        // over and retransmitted. Short so the repeat starts promptly.
        std::chrono::milliseconds hangTime{80};
        // Safety cap on the in-memory burst so a continuous signal cannot grow
        // the buffer without bound.
        double maxCaptureSec = 5.0;
    };

    explicit RepeaterPipeline(SpectrumProcessor *spectrum,
                              QObject *parent = nullptr);
    ~RepeaterPipeline() override;

    // rxDevice and txDevice may be the same handle (a full-duplex device shares
    // one driver handle for both directions).
    bool start(std::shared_ptr<ISDRDevice> rxDevice,
               std::shared_ptr<ISDRDevice> txDevice, const Config &config);
    void stop();
    bool running() const { return m_running; }

signals:
    void started();
    void finished();
    void errorOccurred(const QString &message);
    // Phase transitions for the UI (all queued to the main thread).
    void listening();
    void capturing();
    void transmitting();
    // One captured burst was retransmitted (its length in samples / seconds).
    void repeated(qint64 samples, double seconds);

private:
    void workerLoop(std::stop_token st);

    SpectrumProcessor *m_spectrum;
    std::shared_ptr<ISDRDevice> m_rxDevice;
    std::shared_ptr<ISDRDevice> m_txDevice;
    Config m_config;
    std::jthread m_worker;
    // Duplex mode only: the worker offloads each retransmission here so its RX
    // loop keeps feeding the spectrum. m_txActive gates detection so the
    // repeater never captures its own replay.
    std::jthread m_txThread;
    std::atomic<bool> m_txActive{false};
    bool m_running = false;
};

} // namespace duality
