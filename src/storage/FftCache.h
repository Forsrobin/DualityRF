#pragma once

#include <QString>
#include <QVector>

#include <optional>

namespace duality {

// Append-only binary cache of one average dB spectrum per recording
// (see docs/STORAGE.md). Lets the browser and debug workspace preview a
// recording without streaming the whole IQ file.
class FftCache {
public:
    explicit FftCache(const QString &path);

    bool append(const QString &recordingName, const QVector<float> &db);
    std::optional<QVector<float>> lookup(const QString &recordingName) const;

private:
    QString m_path;
};

} // namespace duality
