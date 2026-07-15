#include "storage/Session.h"

#include "core/Version.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace duality {

namespace {

const QString kMetadataFile = QStringLiteral("metadata.json");
const QString kSessionFile = QStringLiteral("session.json");
const QString kFftCacheFile = QStringLiteral("fft.cache");

bool writeJson(const QString &path, const QJsonDocument &doc)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Session: cannot write" << path << file.errorString();
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

QJsonDocument readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll());
}

} // namespace

Session::Session(QString dir)
    : m_dir(std::move(dir))
    , m_fftCache(m_dir + '/' + kFftCacheFile)
{
}

std::unique_ptr<Session> Session::create(const QString &sessionsRoot)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString name = QStringLiteral("Session_") +
                         now.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    QDir root(sessionsRoot);
    if (!root.mkpath(name)) {
        qWarning() << "Session: cannot create" << root.filePath(name);
        return nullptr;
    }
    auto session = std::unique_ptr<Session>(new Session(root.filePath(name)));
    session->m_created = now;
    session->saveSessionFile();
    session->saveMetadata();
    return session;
}

std::unique_ptr<Session> Session::load(const QString &sessionDir)
{
    if (!QDir(sessionDir).exists())
        return nullptr;
    auto session = std::unique_ptr<Session>(new Session(sessionDir));
    session->loadFiles();
    return session;
}

void Session::loadFiles()
{
    const QJsonObject info =
        readJson(m_dir + '/' + kSessionFile).object();
    m_created = QDateTime::fromString(
        info[QStringLiteral("created")].toString(), Qt::ISODate);

    m_recordings.clear();
    const QJsonArray arr = readJson(m_dir + '/' + kMetadataFile).array();
    for (const QJsonValue &v : arr)
        m_recordings.append(RecordingMetadata::fromJson(v.toObject()));
}

QString Session::name() const
{
    return QDir(m_dir).dirName();
}

QString Session::recordingPath(const RecordingMetadata &meta) const
{
    return m_dir + '/' + meta.file;
}

QString Session::nextRecordingPath() const
{
    return QStringLiteral("%1/recording_%2.iq")
        .arg(m_dir)
        .arg(m_recordings.size() + 1, 3, 10, QLatin1Char('0'));
}

void Session::addRecording(const RecordingMetadata &meta)
{
    m_recordings.append(meta);
    saveMetadata();
}

bool Session::saveMetadata() const
{
    QJsonArray arr;
    for (const RecordingMetadata &m : m_recordings)
        arr.append(m.toJson());
    return writeJson(m_dir + '/' + kMetadataFile, QJsonDocument(arr));
}

bool Session::saveSessionFile() const
{
    const QJsonObject obj{
        {QStringLiteral("created"), m_created.toString(Qt::ISODate)},
        {QStringLiteral("name"), name()},
        {QStringLiteral("notes"), QString()},
        {QStringLiteral("softwareVersion"), QLatin1String(kVersion)},
    };
    return writeJson(m_dir + '/' + kSessionFile, QJsonDocument(obj));
}

} // namespace duality
