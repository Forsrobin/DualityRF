
#pragma once
#include <QObject>
#include <SoapySDR/Device.hpp>

class SDRManager : public QObject {
  Q_OBJECT
public:
  explicit SDRManager(QObject *parent = nullptr);
  bool hasRTLSDR() const;
  bool hasHackRF() const;
  void pollDevices();

signals:
  void devicesUpdated(bool rtlFound, bool hackrfFound);

private:
  bool rtlFound;
  bool hackrfFound;
};
