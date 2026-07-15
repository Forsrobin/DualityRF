#pragma once

#include "core/Types.h"
#include "sdr/ISDRDevice.h"

#include <QObject>

#include <memory>
#include <thread>

namespace duality {

// Replays a recorded IQ file through a transmit-capable device. Speed is
// realized by scaling the device sample rate; the samples themselves are
// sent untouched to preserve the recording.
class PlaybackPipeline : public QObject {
    Q_OBJECT
public:
    struct Params {
        StreamParams stream;   // frequency, gain; sampleRateHz = recording rate
        SampleFormat format = SampleFormat::Cs16;
        double speed = 1.0;    // 0.25 .. 4.0
        bool repeat = false;
    };

    explicit PlaybackPipeline(QObject *parent = nullptr);
    ~PlaybackPipeline() override;

    bool start(std::shared_ptr<ISDRDevice> device, const QString &filePath,
               const Params &params);
    void stop();
    bool running() const { return m_running; }

signals:
    void started();
    void progress(double seconds, double totalSeconds);
    void finished();
    void errorOccurred(const QString &message);

private:
    void workerLoop(std::stop_token st, QString filePath, Params params);

    std::shared_ptr<ISDRDevice> m_device;
    std::jthread m_worker;
    bool m_running = false;
};

} // namespace duality
