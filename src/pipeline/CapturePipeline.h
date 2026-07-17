#pragma once

#include "core/Types.h"
#include "sdr/ISDRDevice.h"
#include "storage/RecordingMetadata.h"

#include <QObject>

#include <memory>
#include <mutex>
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
        // Half-width (± Hz) each auto-capture segment is trimmed to before it
        // is written. 0 disables trimming (keeps the full stream).
        double captureRangeHz = 0.0;
        // Samples kept from just before a segment opens, so a detection that
        // lags the real onset does not clip the start of the transmission.
        qint64 preRollSamples = 0;
        // Segments shorter than this after trimming are discarded rather than
        // written, so brief noise blips never become empty/near-silent files.
        qint64 minSegmentSamples = 0;
    };

    explicit CapturePipeline(SpectrumProcessor *spectrum,
                             QObject *parent = nullptr);
    ~CapturePipeline() override;

    bool start(std::shared_ptr<ISDRDevice> device, const Plan &plan);
    void stop();
    bool running() const { return m_running; }
    bool recording() const { return m_running && !m_plan.outputPath.isEmpty(); }

    // Auto-capture segments carved out of a running monitor stream. Each
    // segment buffers its samples, trims them to plan.captureRangeHz on end,
    // and writes one file. Safe to call from the main thread while running.
    void beginSegment(const QString &outputPath);
    // Close the open segment. dropTrailingSamples is trimmed off the end before
    // writing, so the hang time held open to bridge modulation gaps does not
    // leave a silent tail on the stored file.
    void endSegment(qint64 dropTrailingSamples = 0);

signals:
    void started(bool recording);
    void progress(double seconds, qint64 samples);
    void finished(const duality::RecordingMetadata &meta, bool wasRecording);
    // One auto-capture segment finished and was written to disk.
    void segmentFinished(const duality::RecordingMetadata &meta);
    // An opened segment was closed but too short to keep, so nothing was
    // written. The state machine uses this to re-arm without a recording.
    void segmentDiscarded();
    void errorOccurred(const QString &message);

private:
    void workerLoop(std::stop_token st);
    RecordingMetadata makeMeta(const QString &outputPath,
                               const QDateTime &startUtc, double sampleRateHz,
                               double bandwidthHz, qint64 samples) const;

    SpectrumProcessor *m_spectrum;
    std::shared_ptr<ISDRDevice> m_device;
    Plan m_plan;
    std::jthread m_worker;
    bool m_running = false;

    // Segment commands handed to the worker thread.
    std::mutex m_segMutex;
    QString m_segStartPath;
    bool m_segStopRequested = false;
    qint64 m_segDropTrailing = 0;
};

} // namespace duality
