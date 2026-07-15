#pragma once

#include "storage/Session.h"

#include <QString>
#include <QStringList>

#include <memory>

namespace duality {

// Owns the data root (default ~/DualityRF) and its Sessions/ directory.
class SessionStore {
public:
    explicit SessionStore(QString dataRoot = defaultDataRoot());

    static QString defaultDataRoot();

    QString sessionsDir() const { return m_sessionsDir; }

    // Newest first; absolute paths.
    QStringList listSessionDirs() const;

    std::unique_ptr<Session> createSession() const;

private:
    QString m_sessionsDir;
};

} // namespace duality
