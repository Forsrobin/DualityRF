#pragma once

#include "core/Types.h"
#include "dsp/WaveformGenerator.h"
#include "sdr/ISDRDevice.h"

#include <QObject>

#include <memory>
#include <mutex>
#include <thread>

namespace duality {

// Continuously transmits a generated waveform — the concurrent-TX companion
// to a running capture stage. Runs until stop().
class TransmitPipeline : public QObject {
    Q_OBJECT
public:
    explicit TransmitPipeline(QObject *parent = nullptr);
    ~TransmitPipeline() override;

    bool start(std::shared_ptr<ISDRDevice> device, const StreamParams &params,
               const WaveformConfig &waveform);
    // Swap the generated waveform without interrupting the stream (e.g. when
    // the user changes the waveform type while transmitting). No-op when idle.
    void updateWaveform(const WaveformConfig &waveform);
    void stop();
    bool running() const { return m_running; }

signals:
    void started();
    void finished();
    void errorOccurred(const QString &message);

private:
    void workerLoop(std::stop_token st);
    // Builds a generator for the active stream params; emits errorOccurred and
    // returns nullptr on failure (unloadable file / unsupported type).
    std::unique_ptr<IWaveformGenerator>
    buildGenerator(const WaveformConfig &waveform);

    std::shared_ptr<ISDRDevice> m_device;
    std::jthread m_worker;
    bool m_running = false;
    StreamParams m_params; // active stream params, for rebuilding generators
    std::mutex m_genMutex; // guards m_generator against the live swap
    std::unique_ptr<IWaveformGenerator> m_generator;
};

} // namespace duality
