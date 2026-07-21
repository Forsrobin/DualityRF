#pragma once

#include "sdr/DeviceInfo.h"

#include <QWidget>

#include <optional>

class QComboBox;
class QPushButton;

namespace duality {

class DeviceManager;

// Receiver/transmitter selection backed by DeviceManager discovery.
class DevicePanel : public QWidget {
    Q_OBJECT
public:
    explicit DevicePanel(DeviceManager *manager, QWidget *parent = nullptr);

    std::optional<DeviceInfo> selectedRx() const;
    std::optional<DeviceInfo> selectedTx() const;

signals:
    void selectionChanged();

private slots:
    void onDevicesChanged(const QVector<duality::DeviceInfo> &devices);

private:
    DeviceManager *m_manager;
    QComboBox *m_rxCombo;
    QComboBox *m_txCombo;
    QPushButton *m_rescanButton;
    QVector<DeviceInfo> m_rxDevices;
    QVector<DeviceInfo> m_txDevices;
    // Suppresses persisting selection while combos are being repopulated.
    bool m_refilling = false;
};

} // namespace duality
