#pragma once

#include "core/Types.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace duality {

// Everything needed to interpret and reproduce one IQ recording; stored as
// one entry of the session's metadata.json.
struct RecordingMetadata {
    QString file; // relative to the session directory
    SampleFormat format = SampleFormat::Cs16;

    QString deviceDriver;
    QString deviceLabel;
    QString deviceSerial;

    double frequencyHz = 0.0;
    double sampleRateHz = 0.0;
    double bandwidthHz = 0.0;
    double rxGainDb = 0.0;
    std::optional<double> txGainDb;

    QDateTime startedUtc;
    double durationSec = 0.0;
    qint64 samples = 0;

    QString trigger = QStringLiteral("manual"); // "manual" | "auto"
    QString concurrentTx; // waveform description, empty when none
    QString softwareVersion;

    QJsonObject toJson() const;
    static RecordingMetadata fromJson(const QJsonObject &obj);
};

// Builds metadata for an IQ file that was not produced by this application,
// honoring a PortaPack-style ".TXT" sidecar (center_frequency, sample_rate)
// when present next to a ".C16" file.
RecordingMetadata metadataForForeignFile(const QString &path);

const char *sampleFormatName(SampleFormat format);
std::optional<SampleFormat> sampleFormatFromName(const QString &name);

} // namespace duality
