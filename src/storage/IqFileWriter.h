#pragma once

#include "core/Types.h"

#include <QFile>

#include <span>
#include <vector>

namespace duality {

// Streams cf32 samples to disk in the native cs16 format. Used from the RX
// worker thread; QFile's internal buffering absorbs disk latency spikes.
class IqFileWriter {
public:
    explicit IqFileWriter(const QString &path);
    ~IqFileWriter();

    bool isOpen() const { return m_file.isOpen(); }
    QString path() const { return m_file.fileName(); }
    qint64 samplesWritten() const { return m_samples; }

    bool write(std::span<const Complex> samples);
    bool finalize();

private:
    QFile m_file;
    std::vector<std::int16_t> m_conv;
    qint64 m_samples = 0;
};

} // namespace duality
