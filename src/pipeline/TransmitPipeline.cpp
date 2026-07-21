#include "pipeline/TransmitPipeline.h"

#include "sdr/ITransmitter.h"
#include "storage/IqFileReader.h"
#include "storage/RecordingMetadata.h"

#include <vector>

namespace duality {

namespace {
constexpr std::size_t kChunkSamples = 16384;
// Cap for user-supplied waveform files loaded into memory (64 MiB of cf32).
constexpr qint64 kMaxWaveformSamples = 8 * 1024 * 1024;
}

TransmitPipeline::TransmitPipeline(QObject *parent)
    : QObject(parent)
{
}

TransmitPipeline::~TransmitPipeline()
{
    stop();
}

std::unique_ptr<IWaveformGenerator>
TransmitPipeline::buildGenerator(const WaveformConfig &waveform)
{
    std::vector<Complex> fileSamples;
    if (waveform.type == WaveformType::IqFile) {
        const RecordingMetadata meta = metadataForForeignFile(waveform.filePath);
        fileSamples = IqFileReader::readAll(waveform.filePath, meta.format,
                                            kMaxWaveformSamples);
        if (fileSamples.empty()) {
            emit errorOccurred(
                tr("Cannot load waveform file: %1").arg(waveform.filePath));
            return nullptr;
        }
    }

    auto generator = makeWaveformGenerator(waveform, m_params.sampleRateHz,
                                           std::move(fileSamples));
    if (!generator)
        emit errorOccurred(tr("Unsupported waveform"));
    return generator;
}

bool TransmitPipeline::start(std::shared_ptr<ISDRDevice> device,
                             const StreamParams &params,
                             const WaveformConfig &waveform)
{
    stop();

    ITransmitter *tx = device ? device->transmitter() : nullptr;
    if (!tx) {
        emit errorOccurred(tr("Selected device cannot transmit"));
        return false;
    }
    if (!tx->configure(params)) {
        emit errorOccurred(tr("Transmitter rejected the stream parameters"));
        return false;
    }

    m_params = params;
    auto generator = buildGenerator(waveform);
    if (!generator)
        return false;

    m_device = std::move(device);
    {
        std::lock_guard lock(m_genMutex);
        m_generator = std::move(generator);
    }
    m_worker =
        std::jthread([this](std::stop_token st) { workerLoop(st); });
    m_running = true;
    emit started();
    return true;
}

void TransmitPipeline::updateWaveform(const WaveformConfig &waveform)
{
    if (!m_running)
        return;
    // Build (and load any file) off the lock, then swap it in; keep the
    // current waveform if the new one cannot be built.
    auto generator = buildGenerator(waveform);
    if (!generator)
        return;
    std::lock_guard lock(m_genMutex);
    m_generator = std::move(generator);
}

void TransmitPipeline::stop()
{
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    m_device.reset();
    m_generator.reset();
    m_running = false;
}

void TransmitPipeline::workerLoop(std::stop_token st)
{
    ITransmitter *tx = m_device->transmitter();
    if (!tx->start()) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                m_running = false;
                emit errorOccurred(tr("Failed to start the transmit stream"));
            },
            Qt::QueuedConnection);
        return;
    }

    std::vector<Complex> chunk(kChunkSamples);
    while (!st.stop_requested()) {
        {
            // Only the generation is guarded; the blocking device write stays
            // outside the lock so a live waveform swap never stalls on I/O.
            std::lock_guard lock(m_genMutex);
            m_generator->generate(chunk);
        }
        std::size_t offset = 0;
        while (offset < chunk.size() && !st.stop_requested())
            offset += tx->write(
                std::span(chunk.data() + offset, chunk.size() - offset));
    }
    tx->stop();

    QMetaObject::invokeMethod(
        this,
        [this] {
            m_running = false;
            emit finished();
        },
        Qt::QueuedConnection);
}

} // namespace duality
