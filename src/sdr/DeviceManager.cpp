#include "sdr/DeviceManager.h"

#include "sdr/soapy/SoapyDevice.h"

#include <SoapySDR/Device.hpp>

#include <QDebug>

namespace duality {

namespace {

QString cacheKey(const DeviceInfo &info)
{
    QStringList parts;
    for (auto it = info.args.constBegin(); it != info.args.constEnd(); ++it)
        parts << it.key() + '=' + it.value();
    return parts.join(',');
}

QString kwargValue(const SoapySDR::Kwargs &kwargs, const char *key)
{
    const auto it = kwargs.find(key);
    return it == kwargs.end() ? QString() : QString::fromStdString(it->second);
}

// Enumeration alone does not reveal channel counts, so each candidate is
// briefly opened to classify its RX/TX capabilities.
QVector<DeviceInfo> probeAll()
{
    QVector<DeviceInfo> found;
    for (const SoapySDR::Kwargs &kwargs : SoapySDR::Device::enumerate()) {
        DeviceInfo info;
        info.driver = kwargValue(kwargs, "driver");
        info.label = kwargValue(kwargs, "label");
        info.serial = kwargValue(kwargs, "serial");
        for (const auto &[key, value] : kwargs)
            info.args[QString::fromStdString(key)] =
                QString::fromStdString(value);

        try {
            SoapySDR::Device *dev = SoapySDR::Device::make(kwargs);
            info.canReceive = dev->getNumChannels(SOAPY_SDR_RX) > 0;
            info.canTransmit = dev->getNumChannels(SOAPY_SDR_TX) > 0;
            SoapySDR::Device::unmake(dev);
        } catch (const std::exception &e) {
            qWarning() << "DeviceManager: cannot probe" << info.displayName()
                       << ":" << e.what();
            continue;
        }
        found.push_back(info);
    }
    return found;
}

} // namespace

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DeviceInfo>();
    qRegisterMetaType<QVector<DeviceInfo>>();
}

DeviceManager::~DeviceManager() = default;

void DeviceManager::rescan()
{
    if (m_scanning.exchange(true))
        return;
    emit scanStarted();

    m_scanThread = std::jthread([this] {
        const QVector<DeviceInfo> found = probeAll();
        // Deliver on the main thread; the lambda runs in this object's
        // thread affinity via the queued invocation.
        QMetaObject::invokeMethod(
            this,
            [this, found] {
                m_devices = found;
                m_scanning = false;
                emit devicesChanged(m_devices);
            },
            Qt::QueuedConnection);
    });
}

std::shared_ptr<ISDRDevice> DeviceManager::open(const DeviceInfo &info)
{
    const QString key = cacheKey(info);
    if (auto it = m_openDevices.find(key); it != m_openDevices.end()) {
        if (auto existing = it->second.lock())
            return existing;
    }
    std::shared_ptr<ISDRDevice> dev = SoapyDevice::open(info);
    if (dev)
        m_openDevices[key] = dev;
    return dev;
}

} // namespace duality
