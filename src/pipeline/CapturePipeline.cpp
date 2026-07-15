#include "pipeline/CapturePipeline.h"

#include "core/Version.h"
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

    while (!st.stop_requested()) {
        std::size_t want = chunk.size();
        if (targetSamples >= 0)
            want = std::min<std::size_t>(want,
                                         static_cast<std::size_t>(targetSamples - total));
        const std::size_t got = rx->read(std::span(chunk.data(), want));
        if (got == 0)
            continue; // timeout/overflow; poll the stop token again

        m_spectrum->buffer()->write(std::span(chunk.data(), got));
        if (writer && !writer->write(std::span(chunk.data(), got))) {
            writeFailed = true;
            break;
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
    rx->stop();

    RecordingMetadata meta;
    if (writer) {
        writer->finalize();
        const DeviceInfo &info = m_device->info();
        meta.file = QFileInfo(m_plan.outputPath).fileName();
        meta.format = SampleFormat::Cs16;
        meta.deviceDriver = info.driver;
        meta.deviceLabel = info.label;
        meta.deviceSerial = info.serial;
        meta.frequencyHz = m_plan.params.frequencyHz;
        meta.sampleRateHz = m_plan.params.sampleRateHz;
        meta.bandwidthHz = m_plan.params.bandwidthHz;
        meta.rxGainDb = m_plan.params.gainDb;
        meta.txGainDb = m_plan.txGainDb;
        meta.startedUtc = startedUtc;
        meta.samples = total;
        meta.durationSec = total / m_plan.params.sampleRateHz;
        meta.trigger = m_plan.trigger;
        meta.concurrentTx = m_plan.concurrentTx;
        meta.softwareVersion = QLatin1String(kVersion);
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
