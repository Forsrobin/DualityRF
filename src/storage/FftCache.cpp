#include "storage/FftCache.h"

#include <QDataStream>
#include <QFile>

namespace duality {

namespace {
constexpr quint32 kMagic = 0x54464644; // 'DFFT'
constexpr quint32 kVersion = 1;

void configureStream(QDataStream &s)
{
    s.setByteOrder(QDataStream::LittleEndian);
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);
}
} // namespace

FftCache::FftCache(const QString &path)
    : m_path(path)
{
}

bool FftCache::append(const QString &recordingName, const QVector<float> &db)
{
    QFile file(m_path);
    const bool fresh = !file.exists() || file.size() == 0;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return false;
    QDataStream out(&file);
    configureStream(out);
    if (fresh)
        out << kMagic << kVersion;

    const QByteArray name = recordingName.toUtf8();
    out << static_cast<quint32>(name.size());
    out.writeRawData(name.constData(), name.size());
    out << static_cast<quint32>(db.size());
    for (float v : db)
        out << v;
    return out.status() == QDataStream::Ok;
}

std::optional<QVector<float>> FftCache::lookup(const QString &recordingName) const
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    QDataStream in(&file);
    configureStream(in);

    quint32 magic = 0, version = 0;
    in >> magic >> version;
    if (magic != kMagic || version != kVersion)
        return std::nullopt;

    while (!in.atEnd() && in.status() == QDataStream::Ok) {
        quint32 nameLen = 0;
        in >> nameLen;
        QByteArray name(static_cast<int>(nameLen), Qt::Uninitialized);
        if (in.readRawData(name.data(), name.size()) != name.size())
            return std::nullopt;
        quint32 bins = 0;
        in >> bins;
        if (QString::fromUtf8(name) == recordingName) {
            QVector<float> db(static_cast<int>(bins));
            for (float &v : db)
                in >> v;
            if (in.status() != QDataStream::Ok)
                return std::nullopt;
            return db;
        }
        if (!file.seek(file.pos() + static_cast<qint64>(bins) * sizeof(float)))
            return std::nullopt;
    }
    return std::nullopt;
}

} // namespace duality
