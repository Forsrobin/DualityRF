#pragma once

#include "core/Types.h"
#include "dsp/WaveformGenerator.h"
#include "sdr/ISDRDevice.h"

#include <QObject>

#include <memory>
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
    void stop();
    bool running() const { return m_running; }

signals:
    void started();
    void finished();
    void errorOccurred(const QString &message);

private:
    void workerLoop(std::stop_token st,
                    std::unique_ptr<IWaveformGenerator> generator);

    std::shared_ptr<ISDRDevice> m_device;
    std::jthread m_worker;
    bool m_running = false;
};

} // namespace duality
