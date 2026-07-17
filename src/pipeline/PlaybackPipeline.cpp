#include "pipeline/PlaybackPipeline.h"

#include "sdr/ITransmitter.h"
#include "storage/IqFileReader.h"

#include <chrono>
#include <thread>
#include <vector>

namespace duality {

namespace {
constexpr std::size_t kChunkSamples = 16384;
}

PlaybackPipeline::PlaybackPipeline(QObject *parent)
    : QObject(parent)
{
}

PlaybackPipeline::~PlaybackPipeline()
{
    stop();
}

bool PlaybackPipeline::start(std::shared_ptr<ISDRDevice> device,
                             const QString &filePath, const Params &params)
{
    stop();

    ITransmitter *tx = device ? device->transmitter() : nullptr;
    if (!tx) {
        emit errorOccurred(tr("Selected device cannot transmit"));
        return false;
    }

    StreamParams sp = params.stream;
    sp.sampleRateHz *= params.speed;
    if (!tx->configure(sp)) {
        emit errorOccurred(tr("Transmitter rejected the stream parameters"));
        return false;
    }

    m_device = std::move(device);
    m_worker = std::jthread([this, filePath, params](std::stop_token st) {
        workerLoop(st, filePath, params);
    });
    m_running = true;
    emit started();
    return true;
}

void PlaybackPipeline::stop()
{
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    m_device.reset();
    m_running = false;
}

void PlaybackPipeline::workerLoop(std::stop_token st, QString filePath,
                                  Params params)
{
    using clock = std::chrono::steady_clock;

    auto fail = [this](const QString &msg) {
        QMetaObject::invokeMethod(
            this,
            [this, msg] {
                m_running = false;
                emit errorOccurred(msg);
            },
            Qt::QueuedConnection);
    };

    IqFileReader reader;
    if (!reader.open(filePath, params.format)) {
        fail(tr("Cannot open recording: %1").arg(filePath));
        return;
    }

    ITransmitter *tx = m_device->transmitter();
    if (!tx->start()) {
        fail(tr("Failed to start the transmit stream"));
        return;
    }

    const double effectiveRate = params.stream.sampleRateHz * params.speed;
    const double totalSeconds =
        reader.totalSamples() / params.stream.sampleRateHz;
    std::vector<Complex> chunk(kChunkSamples);
    qint64 sent = 0;
    const auto txStart = clock::now();
    auto lastProgress = txStart;

    const auto emitProgress = [&] {
        const double sec = effectiveRate > 0.0 ? sent / effectiveRate : 0.0;
        QMetaObject::invokeMethod(
            this, [this, sec, totalSeconds] { emit progress(sec, totalSeconds); },
            Qt::QueuedConnection);
    };
    emitProgress(); // show 0 / total immediately, even for a very short file

    while (!st.stop_requested()) {
        const std::size_t got = reader.read(chunk);
        if (got == 0) {
            if (!params.repeat)
                break;
            reader.seekSample(0);
            continue;
        }

        std::size_t offset = 0;
        while (offset < got && !st.stop_requested()) {
            const std::size_t n = tx->write(
                std::span(chunk.data() + offset, got - offset));
            offset += n;
        }
        sent += static_cast<qint64>(got);

        if (clock::now() - lastProgress > std::chrono::milliseconds(200)) {
            lastProgress = clock::now();
            emitProgress();
        }
    }

    // The hardware (e.g. HackRF) accepts samples into a buffer faster than real
    // time and drops whatever is still buffered when the stream is torn down.
    // For a short clip the whole file can be buffered in well under its own
    // duration, so tearing down immediately would transmit nothing. Wait until
    // enough wall-clock has passed to radiate everything we wrote before
    // stopping. A user-requested stop skips the drain.
    if (!st.stop_requested() && effectiveRate > 0.0) {
        const auto playTime = std::chrono::duration<double>(sent / effectiveRate);
        const auto until = txStart +
                           std::chrono::duration_cast<clock::duration>(playTime) +
                           std::chrono::milliseconds(100); // buffer/USB latency
        while (!st.stop_requested() && clock::now() < until) {
            if (clock::now() - lastProgress > std::chrono::milliseconds(200)) {
                lastProgress = clock::now();
                emitProgress();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
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
