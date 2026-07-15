#pragma once

#include <QMap>
#include <QString>

namespace duality {

// Hardware-neutral description of a discovered SDR. `args` are the opaque
// key/value pairs needed to reopen the device through its adapter.
struct DeviceInfo {
    QString driver;
    QString label;
    QString serial;
    bool canReceive = false;
    bool canTransmit = false;
    QMap<QString, QString> args;

    bool fullDuplex() const { return canReceive && canTransmit; }

    QString displayName() const
    {
        return label.isEmpty() ? driver : label;
    }

    bool operator==(const DeviceInfo &other) const
    {
        return args == other.args;
    }
};

} // namespace duality
