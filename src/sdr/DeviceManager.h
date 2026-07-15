#pragma once

#include "sdr/DeviceInfo.h"
#include "sdr/ISDRDevice.h"

#include <QObject>
#include <QVector>

#include <atomic>
#include <map>
#include <memory>
#include <thread>

namespace duality {

// Discovers SoapySDR hardware and hands out opened devices. Discovery runs
// on a worker thread (drivers can take seconds to probe); results arrive via
// devicesChanged(). Opened devices are cached so selecting the same
// full-duplex device for RX and TX shares one driver handle.
class DeviceManager : public QObject {
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    QVector<DeviceInfo> devices() const { return m_devices; }

    // Synchronous; call from the UI only for devices already discovered.
    std::shared_ptr<ISDRDevice> open(const DeviceInfo &info);

public slots:
    void rescan();

signals:
    void scanStarted();
    void devicesChanged(const QVector<duality::DeviceInfo> &devices);

private:
    QVector<DeviceInfo> m_devices;
    std::map<QString, std::weak_ptr<ISDRDevice>> m_openDevices;
    std::atomic<bool> m_scanning{false};
    std::jthread m_scanThread;
};

} // namespace duality
