#pragma once

#include "core/Types.h"

#include <QFile>

#include <span>
#include <vector>

namespace duality {

// Chunked reader for cs16/cf32 IQ files; decodes to cf32 for the pipelines
// and analysis code.
class IqFileReader {
public:
    IqFileReader() = default;

    bool open(const QString &path, SampleFormat format);
    void close();
    bool isOpen() const { return m_file.isOpen(); }

    qint64 totalSamples() const { return m_totalSamples; }
    qint64 position() const { return m_position; }
    bool atEnd() const { return m_position >= m_totalSamples; }

    std::size_t read(std::span<Complex> dst);
    void seekSample(qint64 index);

    // Convenience for analysis/waveform loading; maxSamples caps memory use
    // (-1 = whole file).
    static std::vector<Complex> readAll(const QString &path,
                                        SampleFormat format,
                                        qint64 maxSamples = -1);

private:
    QFile m_file;
    SampleFormat m_format = SampleFormat::Cs16;
    qint64 m_totalSamples = 0;
    qint64 m_position = 0;
    std::vector<char> m_raw;
};

} // namespace duality
