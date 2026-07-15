#include "ui/DevicePanel.h"

#include "sdr/DeviceManager.h"

#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSettings>

namespace duality {

namespace {

// Last selected devices, restored after every scan so a preferred device is
// pre-selected on startup when it is still attached.
const auto kRxKey = QStringLiteral("devices/rx");
const auto kTxKey = QStringLiteral("devices/tx");

QString describeDevice(const DeviceInfo &info)
{
    QString s = info.displayName();
    if (info.fullDuplex())
        s += QStringLiteral(" [duplex]");
    return s;
}

} // namespace

DevicePanel::DevicePanel(DeviceManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
    , m_rxCombo(new QComboBox(this))
    , m_txCombo(new QComboBox(this))
    , m_rescanButton(new QPushButton(tr("RESCAN"), this))
{
    auto *layout = new QFormLayout(this);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addRow(tr("Receiver"), m_rxCombo);
    layout->addRow(tr("Transmitter"), m_txCombo);
    layout->addRow(m_rescanButton);

    m_rxCombo->addItem(tr("(none)"));
    m_txCombo->addItem(tr("(none)"));

    connect(m_rescanButton, &QPushButton::clicked, m_manager,
            &DeviceManager::rescan);
    connect(m_manager, &DeviceManager::scanStarted, this, [this] {
        m_rescanButton->setEnabled(false);
        m_rescanButton->setText(tr("SCANNING…"));
    });
    connect(m_manager, &DeviceManager::devicesChanged, this,
            &DevicePanel::onDevicesChanged);
    connect(m_rxCombo, &QComboBox::currentIndexChanged, this,
            &DevicePanel::selectionChanged);
    connect(m_txCombo, &QComboBox::currentIndexChanged, this,
            &DevicePanel::selectionChanged);
    connect(m_rxCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_refilling)
            QSettings().setValue(kRxKey, m_rxCombo->currentText());
    });
    connect(m_txCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_refilling)
            QSettings().setValue(kTxKey, m_txCombo->currentText());
    });
}

void DevicePanel::onDevicesChanged(const QVector<DeviceInfo> &devices)
{
    m_rescanButton->setEnabled(true);
    m_rescanButton->setText(tr("RESCAN"));

    m_rxDevices.clear();
    m_txDevices.clear();
    for (const DeviceInfo &d : devices) {
        if (d.canReceive)
            m_rxDevices.push_back(d);
        if (d.canTransmit)
            m_txDevices.push_back(d);
    }

    const auto refill = [](QComboBox *combo, const QVector<DeviceInfo> &list,
                           const QString &settingsKey) {
        // Saved selection wins (it also carries across restarts); fall back
        // to the first discovered device.
        const QString preferred =
            QSettings().value(settingsKey, combo->currentText()).toString();
        combo->clear();
        combo->addItem(tr("(none)"));
        for (const DeviceInfo &d : list)
            combo->addItem(describeDevice(d));
        const int idx = combo->findText(preferred);
        combo->setCurrentIndex(idx >= 0 ? idx : (list.isEmpty() ? 0 : 1));
    };
    // Only user-initiated changes persist (see constructor); a scan with the
    // preferred device unplugged must not overwrite the stored preference.
    m_refilling = true;
    refill(m_rxCombo, m_rxDevices, kRxKey);
    refill(m_txCombo, m_txDevices, kTxKey);
    m_refilling = false;
    emit selectionChanged();
}

std::optional<DeviceInfo> DevicePanel::selectedRx() const
{
    const int idx = m_rxCombo->currentIndex() - 1; // slot 0 = (none)
    if (idx < 0 || idx >= m_rxDevices.size())
        return std::nullopt;
    return m_rxDevices[idx];
}

std::optional<DeviceInfo> DevicePanel::selectedTx() const
{
    const int idx = m_txCombo->currentIndex() - 1;
    if (idx < 0 || idx >= m_txDevices.size())
        return std::nullopt;
    return m_txDevices[idx];
}

} // namespace duality
