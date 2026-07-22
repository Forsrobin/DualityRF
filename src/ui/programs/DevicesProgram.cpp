#include "ui/programs/DevicesProgram.h"

#include "sdr/DeviceManager.h"
#include "ui/components/ScanIndicator.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace duality {

namespace {

// Persisted selections, restored after every scan so a preferred device is
// pre-selected on startup when still attached (shared key format with the
// former DevicePanel).
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

DevicesProgram::DevicesProgram(DeviceManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
    , m_scroll(new QScrollArea(this))
    , m_scan(new ScanIndicator(this))
    , m_scanButton(new QPushButton(tr("SCAN FOR DEVICES"), this))
    , m_rxHeader(new QLabel(tr("RECEIVER"), this))
    , m_txHeader(new QLabel(tr("TRANSMITTER"), this))
    , m_rxGroup(new QButtonGroup(this))
    , m_txGroup(new QButtonGroup(this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // Scan control with the animated sweep on top.
    root->addWidget(m_scan);
    m_scanButton->setCursor(Qt::PointingHandCursor);
    root->addWidget(m_scanButton);

    // Scrollable RX / TX lists.
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inner = new QWidget;
    auto *innerLayout = new QVBoxLayout(inner);
    innerLayout->setContentsMargins(0, 0, 0, 0);
    innerLayout->setSpacing(4);

    // Section headers colored to match the top-bar badges (RX blue, TX red).
    m_rxHeader->setStyleSheet(
        QStringLiteral("font-weight: bold; color: #2c6fbf;"));
    innerLayout->addWidget(m_rxHeader);
    auto *rxList = new QWidget(inner);
    m_rxLayout = new QVBoxLayout(rxList);
    m_rxLayout->setContentsMargins(0, 0, 0, 0);
    m_rxLayout->setSpacing(3);
    innerLayout->addWidget(rxList);

    m_txHeader->setStyleSheet(
        QStringLiteral("font-weight: bold; color: #c0392b;"));
    innerLayout->addWidget(m_txHeader);
    auto *txList = new QWidget(inner);
    m_txLayout = new QVBoxLayout(txList);
    m_txLayout->setContentsMargins(0, 0, 0, 0);
    m_txLayout->setSpacing(3);
    innerLayout->addWidget(txList);
    innerLayout->addStretch(1);

    m_scroll->setWidget(inner);
    root->addWidget(m_scroll, 1);

    // Only user clicks persist and emit changes; programmatic re-selection on
    // repopulate must not overwrite the stored preference (id = index + 1,
    // id 0 = "(none)").
    connect(m_rxGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_rxSelected = id - 1;
        QSettings().setValue(
            kRxKey, m_rxSelected >= 0
                        ? describeDevice(m_rxDevices[m_rxSelected])
                        : QString());
        emit selectionChanged();
    });
    connect(m_txGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_txSelected = id - 1;
        QSettings().setValue(
            kTxKey, m_txSelected >= 0
                        ? describeDevice(m_txDevices[m_txSelected])
                        : QString());
        emit selectionChanged();
    });

    connect(m_scanButton, &QPushButton::clicked, m_manager,
            &DeviceManager::rescan);
    connect(m_manager, &DeviceManager::scanStarted, this, [this] {
        m_scan->setScanning(true);
        m_scanButton->setEnabled(false);
        m_scanButton->setText(tr("SCANNING…"));
    });
    connect(m_manager, &DeviceManager::devicesChanged, this,
            &DevicesProgram::onDevicesChanged);

    // Start with just "(none)" rows until the first scan reports back.
    populate(m_rxLayout, m_rxGroup, m_rxDevices, m_rxSelected, kRxKey);
    populate(m_txLayout, m_txGroup, m_txDevices, m_txSelected, kTxKey);
}

void DevicesProgram::populate(QVBoxLayout *layout, QButtonGroup *group,
                              const QVector<DeviceInfo> &devices, int &selected,
                              const QString &settingsKey)
{
    // Clear existing rows.
    for (QAbstractButton *b : group->buttons()) {
        group->removeButton(b);
        b->deleteLater();
    }
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    const auto addRow = [&](int id, const QString &text) {
        auto *b = new QPushButton(text);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QStringLiteral("text-align: left; padding: 6px 8px;"));
        group->addButton(b, id);
        layout->addWidget(b);
    };
    addRow(0, tr("(none)"));
    for (int i = 0; i < devices.size(); ++i)
        addRow(i + 1, describeDevice(devices[i]));

    // Restore the saved preference by name; fall back to none.
    const QString preferred = QSettings().value(settingsKey).toString();
    selected = -1;
    for (int i = 0; i < devices.size(); ++i) {
        if (describeDevice(devices[i]) == preferred) {
            selected = i;
            break;
        }
    }
    if (QAbstractButton *b = group->button(selected + 1))
        b->setChecked(true);
}

void DevicesProgram::onDevicesChanged(const QVector<DeviceInfo> &devices)
{
    m_scan->setScanning(false);
    m_scanButton->setEnabled(true);
    m_scanButton->setText(tr("SCAN FOR DEVICES"));

    m_rxDevices.clear();
    m_txDevices.clear();
    for (const DeviceInfo &d : devices) {
        if (d.canReceive)
            m_rxDevices.push_back(d);
        if (d.canTransmit)
            m_txDevices.push_back(d);
    }
    populate(m_rxLayout, m_rxGroup, m_rxDevices, m_rxSelected, kRxKey);
    populate(m_txLayout, m_txGroup, m_txDevices, m_txSelected, kTxKey);
    emit selectionChanged();
}

std::optional<DeviceInfo> DevicesProgram::selectedRx() const
{
    if (m_rxSelected < 0 || m_rxSelected >= m_rxDevices.size())
        return std::nullopt;
    return m_rxDevices[m_rxSelected];
}

std::optional<DeviceInfo> DevicesProgram::selectedTx() const
{
    if (m_txSelected < 0 || m_txSelected >= m_txDevices.size())
        return std::nullopt;
    return m_txDevices[m_txSelected];
}

void DevicesProgram::focusReceiver()
{
    m_scroll->ensureWidgetVisible(m_rxHeader);
}

void DevicesProgram::focusTransmitter()
{
    m_scroll->ensureWidgetVisible(m_txHeader);
}

} // namespace duality
