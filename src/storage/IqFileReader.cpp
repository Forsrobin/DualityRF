#include "storage/IqFileReader.h"

#include <QDebug>

#include <cstring>

namespace duality {

bool IqFileReader::open(const QString &path, SampleFormat format)
{
    close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly)) {
        qWarning() << "IqFileReader: cannot open" << path
                   << m_file.errorString();
        return false;
    }
    m_format = format;
    m_totalSamples =
        m_file.size() / static_cast<qint64>(bytesPerSample(format));
    m_position = 0;
    return true;
}

void IqFileReader::close()
{
    if (m_file.isOpen())
        m_file.close();
    m_totalSamples = 0;
    m_position = 0;
}

std::size_t IqFileReader::read(std::span<Complex> dst)
{
    if (!m_file.isOpen() || dst.empty())
        return 0;

    const std::size_t sampleBytes = bytesPerSample(m_format);
    m_raw.resize(dst.size() * sampleBytes);
    const qint64 got = m_file.read(m_raw.data(),
                                   static_cast<qint64>(m_raw.size()));
    if (got <= 0)
        return 0;
    const std::size_t n = static_cast<std::size_t>(got) / sampleBytes;

    if (m_format == SampleFormat::Cf32) {
        std::memcpy(dst.data(), m_raw.data(), n * sampleBytes);
    } else {
        const auto *src = reinterpret_cast<const std::int16_t *>(m_raw.data());
        constexpr float scale = 1.0f / 32768.0f;
        for (std::size_t i = 0; i < n; ++i)
            dst[i] = {src[2 * i] * scale, src[2 * i + 1] * scale};
    }
    m_position += static_cast<qint64>(n);
    return n;
}

void IqFileReader::seekSample(qint64 index)
{
    if (!m_file.isOpen())
        return;
    index = std::clamp<qint64>(index, 0, m_totalSamples);
    m_file.seek(index * static_cast<qint64>(bytesPerSample(m_format)));
    m_position = index;
}

std::vector<Complex> IqFileReader::readAll(const QString &path,
                                           SampleFormat format,
                                           qint64 maxSamples)
{
    std::vector<Complex> out;
    IqFileReader reader;
    if (!reader.open(path, format))
        return out;
    qint64 want = reader.totalSamples();
    if (maxSamples >= 0)
        want = std::min(want, maxSamples);
    out.resize(static_cast<std::size_t>(want));
    std::size_t filled = 0;
    while (filled < out.size()) {
        const std::size_t got = reader.read(
            std::span(out.data() + filled, out.size() - filled));
        if (got == 0)
            break;
        filled += got;
    }
    out.resize(filled);
    return out;
}

} // namespace duality
