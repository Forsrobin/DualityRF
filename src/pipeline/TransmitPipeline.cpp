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

    std::vector<Complex> fileSamples;
    if (waveform.type == WaveformType::IqFile) {
        const RecordingMetadata meta = metadataForForeignFile(waveform.filePath);
        fileSamples = IqFileReader::readAll(waveform.filePath, meta.format,
                                            kMaxWaveformSamples);
        if (fileSamples.empty()) {
            emit errorOccurred(
                tr("Cannot load waveform file: %1").arg(waveform.filePath));
            return false;
        }
    }

    auto generator = makeWaveformGenerator(waveform, params.sampleRateHz,
                                           std::move(fileSamples));
    if (!generator) {
        emit errorOccurred(tr("Unsupported waveform"));
        return false;
    }

    m_device = std::move(device);
    m_worker = std::jthread(
        [this, gen = std::move(generator)](std::stop_token st) mutable {
            workerLoop(st, std::move(gen));
        });
    m_running = true;
    emit started();
    return true;
}

void TransmitPipeline::stop()
{
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    m_device.reset();
    m_running = false;
}

void TransmitPipeline::workerLoop(std::stop_token st,
                                  std::unique_ptr<IWaveformGenerator> generator)
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
        generator->generate(chunk);
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
