#pragma once

#include "sdr/DeviceInfo.h"

#include <QWidget>

#include <optional>

class QButtonGroup;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace duality {

class DeviceManager;
class ScanIndicator;

// The "Devicees" program: the app-wide receiver/transmitter selector. Shows the
// discovered devices as two touch-friendly, selectable lists (RX and TX) with
// an animated scan. It is the single source of truth for device selection —
// pipelines read selectedRx()/selectedTx() and the program top bars mirror it.
class DevicesProgram : public QWidget {
    Q_OBJECT
public:
    explicit DevicesProgram(DeviceManager *manager, QWidget *parent = nullptr);

    std::optional<DeviceInfo> selectedRx() const;
    std::optional<DeviceInfo> selectedTx() const;

    // Scroll a section into view (used when opened from a top-bar badge).
    void focusReceiver();
    void focusTransmitter();

signals:
    void selectionChanged();

private:
    void onDevicesChanged(const QVector<duality::DeviceInfo> &devices);
    // Rebuild one section's selectable rows, restoring the saved preference.
    void populate(QVBoxLayout *layout, QButtonGroup *group,
                  const QVector<DeviceInfo> &devices, int &selected,
                  const QString &settingsKey);

    DeviceManager *m_manager;
    QScrollArea *m_scroll;
    ScanIndicator *m_scan;
    QPushButton *m_scanButton;
    QLabel *m_rxHeader;
    QLabel *m_txHeader;
    QVBoxLayout *m_rxLayout;
    QVBoxLayout *m_txLayout;
    QButtonGroup *m_rxGroup;
    QButtonGroup *m_txGroup;
    QVector<DeviceInfo> m_rxDevices;
    QVector<DeviceInfo> m_txDevices;
    int m_rxSelected = -1; // index into m_rxDevices; -1 = none
    int m_txSelected = -1;
};

} // namespace duality
