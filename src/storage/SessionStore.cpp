#include "storage/SessionStore.h"

#include <QDir>
#include <QStandardPaths>

namespace duality {

SessionStore::SessionStore(QString dataRoot)
    : m_sessionsDir(dataRoot + QStringLiteral("/Sessions"))
{
    QDir().mkpath(m_sessionsDir);
}

QString SessionStore::defaultDataRoot()
{
    return QStandardPaths::writableLocation(
               QStandardPaths::GenericDataLocation) +
           QStringLiteral("/duality-rf");
}

QStringList SessionStore::listSessionDirs() const
{
    const QDir dir(m_sessionsDir);
    QStringList out;
    const QStringList names =
        dir.entryList({QStringLiteral("Session_*")},
                      QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (auto it = names.crbegin(); it != names.crend(); ++it)
        out << dir.filePath(*it);
    return out;
}

std::unique_ptr<Session> SessionStore::createSession() const
{
    return Session::create(m_sessionsDir);
}

bool SessionStore::deleteSession(const QString &sessionDir) const
{
    // Guard against deleting anything outside the managed Sessions directory.
    const QString canonical = QDir(sessionDir).canonicalPath();
    const QString root = QDir(m_sessionsDir).canonicalPath();
    if (canonical.isEmpty() || root.isEmpty() ||
        !canonical.startsWith(root + '/'))
        return false;
    return QDir(canonical).removeRecursively();
}

} // namespace duality
