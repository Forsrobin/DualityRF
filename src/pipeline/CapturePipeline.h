#pragma once

#include "core/Types.h"
#include "sdr/ISDRDevice.h"
#include "storage/RecordingMetadata.h"

#include <QObject>

#include <memory>
#include <thread>

namespace duality {

class SpectrumProcessor;

// Runs one receive stage on a worker thread: streams samples into the live
// spectrum and, when recording, into an IQ file. The session commit (adding
// the metadata emitted by finished()) is the caller's job, keeping this
// class free of session bookkeeping.
class CapturePipeline : public QObject {
    Q_OBJECT
public:
    struct Plan {
        StreamParams params;
        double durationSec = 0.0;  // 0 = run until stop()
        QString outputPath;        // empty = monitor only
        QString trigger = QStringLiteral("manual");
        QString concurrentTx;      // description for metadata, may be empty
        std::optional<double> txGainDb;
    };

    explicit CapturePipeline(SpectrumProcessor *spectrum,
                             QObject *parent = nullptr);
    ~CapturePipeline() override;

    bool start(std::shared_ptr<ISDRDevice> device, const Plan &plan);
    void stop();
    bool running() const { return m_running; }
    bool recording() const { return m_running && !m_plan.outputPath.isEmpty(); }

signals:
    void started(bool recording);
    void progress(double seconds, qint64 samples);
    void finished(const duality::RecordingMetadata &meta, bool wasRecording);
    void errorOccurred(const QString &message);

private:
    void workerLoop(std::stop_token st);

    SpectrumProcessor *m_spectrum;
    std::shared_ptr<ISDRDevice> m_device;
    Plan m_plan;
    std::jthread m_worker;
    bool m_running = false;
};

} // namespace duality
