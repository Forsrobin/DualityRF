#pragma once

#include "core/RingBuffer.h"
#include "core/Types.h"
#include "dsp/Window.h"

#include <QObject>
#include <QVector>

#include <memory>
#include <thread>

namespace duality {

// Live spectrum worker. A producer (capture pipeline) pushes raw samples
// into buffer(); this object's worker thread windows, FFTs and averages
// them, and emits fftshifted dB rows at a bounded rate. Display overruns
// drop samples here and never affect recording.
class SpectrumProcessor : public QObject {
    Q_OBJECT
public:
    struct Config {
        int fftSize = 2048;
        WindowType window = WindowType::Hann;
        float averaging = 0.6f; // EMA weight of the previous row [0..1)
        int maxRowsPerSec = 30;
    };

    explicit SpectrumProcessor(QObject *parent = nullptr);
    ~SpectrumProcessor() override;

    void start(const Config &config);
    void stop();
    bool running() const { return m_running; }

    // Valid between start() and stop(); single producer only.
    SpscRingBuffer<Complex> *buffer() { return m_buffer.get(); }

signals:
    void spectrumReady(const QVector<float> &dbRow);

private:
    void workerLoop(std::stop_token st, Config config);

    std::unique_ptr<SpscRingBuffer<Complex>> m_buffer;
    std::jthread m_worker;
    bool m_running = false;
};

} // namespace duality
