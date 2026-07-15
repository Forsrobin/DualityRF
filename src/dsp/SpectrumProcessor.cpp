#include "dsp/SpectrumProcessor.h"

#include "dsp/FftEngine.h"
#include "dsp/SpectrumAccumulator.h"

#include <chrono>

namespace duality {

SpectrumProcessor::SpectrumProcessor(QObject *parent)
    : QObject(parent)
{
}

SpectrumProcessor::~SpectrumProcessor()
{
    stop();
}

void SpectrumProcessor::start(const Config &config)
{
    stop();
    m_buffer = std::make_unique<SpscRingBuffer<Complex>>(
        static_cast<std::size_t>(config.fftSize) * 16);
    m_worker = std::jthread(
        [this, config](std::stop_token st) { workerLoop(st, config); });
    m_running = true;
}

void SpectrumProcessor::stop()
{
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_worker.join();
    }
    m_running = false;
}

void SpectrumProcessor::workerLoop(std::stop_token st, Config config)
{
    using clock = std::chrono::steady_clock;

    FftEngine fft(config.fftSize);
    const std::vector<float> window = makeWindow(config.window, config.fftSize);
    std::vector<Complex> block(config.fftSize);
    std::vector<Complex> spectrum(config.fftSize);
    QVector<float> row(config.fftSize);
    QVector<float> averaged;

    const auto minInterval =
        std::chrono::microseconds(1000000 / std::max(1, config.maxRowsPerSec));
    auto lastEmit = clock::now() - minInterval;

    const double norm = 1.0 / (static_cast<double>(config.fftSize) * config.fftSize);
    const float alpha = config.averaging;
    const int n = config.fftSize;

    while (!st.stop_requested()) {
        const std::size_t got =
            m_buffer->read(std::span(block.data(), block.size()));
        if (got < block.size()) {
            // Not enough samples yet; drop the partial read (display only)
            // and let the buffer refill.
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (clock::now() - lastEmit < minInterval)
            continue; // stay real-time by discarding excess blocks

        for (int i = 0; i < n; ++i)
            block[i] *= window[i];
        fft.execute(block, spectrum);

        for (int i = 0; i < n; ++i) {
            const double p = std::norm(spectrum[(i + n / 2) % n]) * norm;
            row[i] = powerToDb(p);
        }

        if (averaged.size() != row.size()) {
            averaged = row;
        } else {
            for (int i = 0; i < n; ++i)
                averaged[i] = alpha * averaged[i] + (1.0f - alpha) * row[i];
        }

        lastEmit = clock::now();
        emit spectrumReady(averaged);
    }
}

} // namespace duality
