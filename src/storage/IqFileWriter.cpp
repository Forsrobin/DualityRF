#include "storage/IqFileWriter.h"

#include <QDebug>

#include <algorithm>
#include <cmath>

namespace duality {

IqFileWriter::IqFileWriter(const QString &path)
    : m_file(path)
{
    if (!m_file.open(QIODevice::WriteOnly))
        qWarning() << "IqFileWriter: cannot open" << path
                   << m_file.errorString();
}

IqFileWriter::~IqFileWriter()
{
    finalize();
}

bool IqFileWriter::write(std::span<const Complex> samples)
{
    if (!m_file.isOpen())
        return false;

    m_conv.resize(samples.size() * 2);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float re = std::clamp(samples[i].real(), -1.0f, 1.0f);
        const float im = std::clamp(samples[i].imag(), -1.0f, 1.0f);
        m_conv[2 * i] = static_cast<std::int16_t>(std::lrintf(re * 32767.0f));
        m_conv[2 * i + 1] =
            static_cast<std::int16_t>(std::lrintf(im * 32767.0f));
    }

    const qint64 bytes =
        static_cast<qint64>(m_conv.size() * sizeof(std::int16_t));
    if (m_file.write(reinterpret_cast<const char *>(m_conv.data()), bytes) !=
        bytes) {
        qWarning() << "IqFileWriter: short write on" << m_file.fileName()
                   << m_file.errorString();
        return false;
    }
    m_samples += static_cast<qint64>(samples.size());
    return true;
}

bool IqFileWriter::finalize()
{
    if (!m_file.isOpen())
        return false;
    const bool ok = m_file.flush();
    m_file.close();
    return ok;
}

} // namespace duality
