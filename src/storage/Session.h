#pragma once

#include "storage/FftCache.h"
#include "storage/RecordingMetadata.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <memory>

namespace duality {

// One session directory: numbered recordings, metadata.json, session.json
// and the fft.cache (see docs/STORAGE.md).
class Session {
public:
    static std::unique_ptr<Session> create(const QString &sessionsRoot);
    static std::unique_ptr<Session> load(const QString &sessionDir);

    QString dir() const { return m_dir; }
    QString name() const;
    QDateTime created() const { return m_created; }

    const QList<RecordingMetadata> &recordings() const { return m_recordings; }
    QString recordingPath(const RecordingMetadata &meta) const;

    // Absolute path the next recording should be written to
    // (recording_NNN.iq); the file becomes part of the session only once
    // addRecording() commits its metadata.
    QString nextRecordingPath() const;

    void addRecording(const RecordingMetadata &meta);

    FftCache &fftCache() { return m_fftCache; }

private:
    explicit Session(QString dir);
    bool saveMetadata() const;
    bool saveSessionFile() const;
    void loadFiles();

    QString m_dir;
    QDateTime m_created;
    QList<RecordingMetadata> m_recordings;
    FftCache m_fftCache;
};

} // namespace duality
