#include "storage/RecordingMetadata.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonValue>
#include <QTextStream>

namespace duality {

const char *sampleFormatName(SampleFormat format)
{
    return format == SampleFormat::Cs16 ? "cs16" : "cf32";
}

std::optional<SampleFormat> sampleFormatFromName(const QString &name)
{
    if (name == QLatin1String("cs16"))
        return SampleFormat::Cs16;
    if (name == QLatin1String("cf32"))
        return SampleFormat::Cf32;
    return std::nullopt;
}

QJsonObject RecordingMetadata::toJson() const
{
    QJsonObject dev{
        {QStringLiteral("driver"), deviceDriver},
        {QStringLiteral("label"), deviceLabel},
        {QStringLiteral("serial"), deviceSerial},
    };
    QJsonObject obj{
        {QStringLiteral("file"), file},
        {QStringLiteral("format"), sampleFormatName(format)},
        {QStringLiteral("device"), dev},
        {QStringLiteral("frequencyHz"), frequencyHz},
        {QStringLiteral("sampleRateHz"), sampleRateHz},
        {QStringLiteral("bandwidthHz"), bandwidthHz},
        {QStringLiteral("rxGainDb"), rxGainDb},
        {QStringLiteral("txGainDb"),
         txGainDb ? QJsonValue(*txGainDb) : QJsonValue()},
        {QStringLiteral("startedUtc"), startedUtc.toString(Qt::ISODate)},
        {QStringLiteral("durationSec"), durationSec},
        {QStringLiteral("samples"), samples},
        {QStringLiteral("trigger"), trigger},
        {QStringLiteral("concurrentTx"),
         concurrentTx.isEmpty() ? QJsonValue() : QJsonValue(concurrentTx)},
        {QStringLiteral("softwareVersion"), softwareVersion},
    };
    return obj;
}

RecordingMetadata RecordingMetadata::fromJson(const QJsonObject &obj)
{
    RecordingMetadata m;
    m.file = obj[QStringLiteral("file")].toString();
    m.format = sampleFormatFromName(obj[QStringLiteral("format")].toString())
                   .value_or(SampleFormat::Cs16);
    const QJsonObject dev = obj[QStringLiteral("device")].toObject();
    m.deviceDriver = dev[QStringLiteral("driver")].toString();
    m.deviceLabel = dev[QStringLiteral("label")].toString();
    m.deviceSerial = dev[QStringLiteral("serial")].toString();
    m.frequencyHz = obj[QStringLiteral("frequencyHz")].toDouble();
    m.sampleRateHz = obj[QStringLiteral("sampleRateHz")].toDouble();
    m.bandwidthHz = obj[QStringLiteral("bandwidthHz")].toDouble();
    m.rxGainDb = obj[QStringLiteral("rxGainDb")].toDouble();
    if (obj[QStringLiteral("txGainDb")].isDouble())
        m.txGainDb = obj[QStringLiteral("txGainDb")].toDouble();
    m.startedUtc = QDateTime::fromString(
        obj[QStringLiteral("startedUtc")].toString(), Qt::ISODate);
    m.durationSec = obj[QStringLiteral("durationSec")].toDouble();
    m.samples =
        static_cast<qint64>(obj[QStringLiteral("samples")].toDouble());
    m.trigger = obj[QStringLiteral("trigger")].toString();
    m.concurrentTx = obj[QStringLiteral("concurrentTx")].toString();
    m.softwareVersion = obj[QStringLiteral("softwareVersion")].toString();
    return m;
}

RecordingMetadata metadataForForeignFile(const QString &path)
{
    RecordingMetadata m;
    const QFileInfo fi(path);
    m.file = fi.fileName();
    const QString suffix = fi.suffix().toLower();
    m.format = (suffix == QLatin1String("cf32") || suffix == QLatin1String("f32"))
                   ? SampleFormat::Cf32
                   : SampleFormat::Cs16;
    m.samples = fi.size() / static_cast<qint64>(bytesPerSample(m.format));

    QFile sidecar(fi.path() + '/' + fi.completeBaseName() +
                  QStringLiteral(".TXT"));
    if (sidecar.exists() && sidecar.open(QIODevice::ReadOnly)) {
        QTextStream in(&sidecar);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            const auto parts = line.split('=');
            if (parts.size() != 2)
                continue;
            if (parts[0] == QLatin1String("center_frequency"))
                m.frequencyHz = parts[1].toDouble();
            else if (parts[0] == QLatin1String("sample_rate"))
                m.sampleRateHz = parts[1].toDouble();
        }
    }
    if (m.sampleRateHz > 0.0)
        m.durationSec = static_cast<double>(m.samples) / m.sampleRateHz;
    return m;
}

} // namespace duality
