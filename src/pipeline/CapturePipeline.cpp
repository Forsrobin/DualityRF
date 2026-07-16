#include "pipeline/CapturePipeline.h"

#include "core/Version.h"
#include "dsp/BandTrimmer.h"
#include "dsp/SpectrumProcessor.h"
#include "sdr/IReceiver.h"
#include "storage/IqFileWriter.h"

#include <QDateTime>
#include <QFileInfo>

#include <chrono>
#include <vector>

namespace duality {

namespace {
constexpr std::size_t kChunkSamples = 16384;
// Safety cap on a single auto-capture segment held in memory before it is
// trimmed and written (~64M samples). A signal longer than this is split.
constexpr qint64 kMaxSegmentSamples = 64 * 1024 * 1024;
}

CapturePipeline::CapturePipeline(SpectrumProcessor *spectrum, QObject *parent)
    : QObject(parent)
    , m_spectrum(spectrum)
{
    qRegisterMetaType<RecordingMetadata>("duality::RecordingMetadata");
}

CapturePipeline::~CapturePipeline()
{
    stop();
}

bool CapturePipeline::start(std::shared_ptr<ISDRDevice> device,
                            const Plan &plan)
{
    stop();

    IReceiver *rx = device ? device->receiver() : nullptr;
    if (!rx) {
        emit errorOccurred(tr("Selected device cannot receive"));
        return false;
    }
    if (!rx->configure(plan.params)) {
        emit errorOccurred(tr("Receiver rejected the stream parameters"));
        return false;
    }

    m_device = std::move(device);
    m_plan = plan;

    SpectrumProcessor::Config specCfg;
    m_spectrum->start(specCfg);

    m_worker = std::jthread([this](std::stop_token st) { workerLoop(st); });
    m_running = true;
    emit started(!m_plan.outputPath.isEmpty());
    return true;
}

void CapturePipeline::stop()
{
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    if (m_spectrum->running())
        m_spectrum->stop();
    m_device.reset();
    m_running = false;
    std::lock_guard lock(m_segMutex);
    m_segStartPath.clear();
    m_segStopRequested = false;
}

void CapturePipeline::beginSegment(const QString &outputPath)
{
    std::lock_guard lock(m_segMutex);
    m_segStartPath = outputPath;
    m_segStopRequested = false;
}

void CapturePipeline::endSegment()
{
    std::lock_guard lock(m_segMutex);
    m_segStopRequested = true;
}

RecordingMetadata CapturePipeline::makeMeta(const QString &outputPath,
                                            const QDateTime &startUtc,
                                            double sampleRateHz,
                                            double bandwidthHz,
                                            qint64 samples) const
{
    RecordingMetadata meta;
    const DeviceInfo &info = m_device->info();
    meta.file = QFileInfo(outputPath).fileName();
    meta.format = SampleFormat::Cs16;
    meta.deviceDriver = info.driver;
    meta.deviceLabel = info.label;
    meta.deviceSerial = info.serial;
    meta.frequencyHz = m_plan.params.frequencyHz;
    meta.sampleRateHz = sampleRateHz;
    meta.bandwidthHz = bandwidthHz;
    meta.rxGainDb = m_plan.params.gainDb;
    meta.txGainDb = m_plan.txGainDb;
    meta.startedUtc = startUtc;
    meta.samples = samples;
    meta.durationSec = sampleRateHz > 0.0 ? samples / sampleRateHz : 0.0;
    meta.trigger = m_plan.trigger;
    meta.concurrentTx = m_plan.concurrentTx;
    meta.softwareVersion = QLatin1String(kVersion);
    return meta;
}

void CapturePipeline::workerLoop(std::stop_token st)
{
    using clock = std::chrono::steady_clock;

    IReceiver *rx = m_device->receiver();
    if (!rx->start()) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                m_running = false;
                emit errorOccurred(tr("Failed to start the receive stream"));
            },
            Qt::QueuedConnection);
        return;
    }

    const bool record = !m_plan.outputPath.isEmpty();
    std::unique_ptr<IqFileWriter> writer;
    if (record)
        writer = std::make_unique<IqFileWriter>(m_plan.outputPath);

    // A manual recording is trimmed to the capture range as it streams to disk,
    // so the stored file only contains the band of interest (the live spectrum
    // still gets the full-rate samples). Auto segments trim separately below.
    BandTrimmer mainTrimmer(m_plan.params.sampleRateHz, m_plan.captureRangeHz);
    const bool trimMain = record && !mainTrimmer.passthrough();
    if (trimMain)
        mainTrimmer.resetStream();
    qint64 outSamples = 0;

    const qint64 targetSamples =
        m_plan.durationSec > 0.0
            ? static_cast<qint64>(m_plan.durationSec *
                                  m_plan.params.sampleRateHz)
            : -1;

    std::vector<Complex> chunk(kChunkSamples);
    qint64 total = 0;
    const QDateTime startedUtc = QDateTime::currentDateTimeUtc();
    auto lastProgress = clock::now();
    bool writeFailed = false;

    // Auto-capture segment carved out of the running stream.
    bool segActive = false;
    QString segPath;
    QDateTime segStart;
    std::vector<Complex> segBuf;

    const auto finalizeSegment = [&] {
        if (!segActive)
            return;
        segActive = false;
        const BandTrimmer trimmer(m_plan.params.sampleRateHz,
                                  m_plan.captureRangeHz);
        const std::vector<Complex> trimmed = trimmer.process(segBuf);
        const double outRate = trimmer.outputRateHz();
        const double bwHz = trimmer.passthrough() ? m_plan.params.bandwidthHz
                                                  : trimmer.bandwidthHz();
        {
            IqFileWriter segWriter(segPath);
            segWriter.write(std::span(trimmed.data(), trimmed.size()));
            segWriter.finalize();
        }
        RecordingMetadata meta =
            makeMeta(segPath, segStart, outRate, bwHz,
                     static_cast<qint64>(trimmed.size()));
        segBuf.clear();
        segBuf.shrink_to_fit();
        QMetaObject::invokeMethod(
            this, [this, meta] { emit segmentFinished(meta); },
            Qt::QueuedConnection);
    };

    while (!st.stop_requested()) {
        // Pick up any pending segment command before touching samples.
        QString startPath;
        bool stopSeg = false;
        {
            std::lock_guard lock(m_segMutex);
            startPath = m_segStartPath;
            m_segStartPath.clear();
            stopSeg = m_segStopRequested;
            m_segStopRequested = false;
        }
        if (!startPath.isEmpty()) {
            finalizeSegment(); // close any segment still open
            segActive = true;
            segPath = startPath;
            segStart = QDateTime::currentDateTimeUtc();
            segBuf.clear();
        }
        if (stopSeg)
            finalizeSegment();

        std::size_t want = chunk.size();
        if (targetSamples >= 0)
            want = std::min<std::size_t>(want,
                                         static_cast<std::size_t>(targetSamples - total));
        const std::size_t got = rx->read(std::span(chunk.data(), want));
        if (got == 0)
            continue; // timeout/overflow; poll the stop token again

        m_spectrum->buffer()->write(std::span(chunk.data(), got));
        if (writer) {
            bool ok = true;
            if (trimMain) {
                const std::vector<Complex> t =
                    mainTrimmer.processStream(std::span(chunk.data(), got));
                if (!t.empty()) {
                    ok = writer->write(std::span(t.data(), t.size()));
                    outSamples += static_cast<qint64>(t.size());
                }
            } else {
                ok = writer->write(std::span(chunk.data(), got));
            }
            if (!ok) {
                writeFailed = true;
                break;
            }
        }
        if (segActive) {
            segBuf.insert(segBuf.end(), chunk.data(), chunk.data() + got);
            if (static_cast<qint64>(segBuf.size()) >= kMaxSegmentSamples)
                finalizeSegment(); // safety valve; splits an overlong signal
        }
        total += static_cast<qint64>(got);

        if (clock::now() - lastProgress > std::chrono::milliseconds(200)) {
            lastProgress = clock::now();
            const double sec = total / m_plan.params.sampleRateHz;
            QMetaObject::invokeMethod(
                this, [this, sec, total] { emit progress(sec, total); },
                Qt::QueuedConnection);
        }
        if (targetSamples >= 0 && total >= targetSamples)
            break;
    }
    finalizeSegment(); // commit a segment left open at stop
    rx->stop();

    RecordingMetadata meta;
    if (writer) {
        if (trimMain && !writeFailed) {
            const std::vector<Complex> tail = mainTrimmer.flushStream();
            if (!tail.empty()) {
                writer->write(std::span(tail.data(), tail.size()));
                outSamples += static_cast<qint64>(tail.size());
            }
        }
        writer->finalize();
        meta = trimMain
                   ? makeMeta(m_plan.outputPath, startedUtc,
                              mainTrimmer.outputRateHz(),
                              mainTrimmer.bandwidthHz(), outSamples)
                   : makeMeta(m_plan.outputPath, startedUtc,
                              m_plan.params.sampleRateHz,
                              m_plan.params.bandwidthHz, total);
    }

    QMetaObject::invokeMethod(
        this,
        [this, meta, record, writeFailed] {
            m_running = false;
            if (writeFailed)
                emit errorOccurred(tr("Recording aborted: disk write failed"));
            emit finished(meta, record && !writeFailed);
        },
        Qt::QueuedConnection);
}

} // namespace duality
