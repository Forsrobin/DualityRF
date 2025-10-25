
#include "SDRManager.h"
#include <SoapySDR/Device.hpp>

SDRManager::SDRManager(QObject *parent)
    : QObject(parent), rtlFound(false), hackrfFound(false) {}

bool SDRManager::hasRTLSDR() const { return rtlFound; }
bool SDRManager::hasHackRF() const { return hackrfFound; }

void SDRManager::pollDevices() {
  auto results = SoapySDR::Device::enumerate();
  bool rtl = false, hack = false;

  for (auto &r : results) {
    if (r.find("driver") == r.end())
      continue;
    if (r.at("driver") == "rtlsdr")
      rtl = true;
    if (r.at("driver") == "hackrf")
      hack = true;
  }

  if (rtl != rtlFound || hack != hackrfFound) {
    rtlFound = rtl;
    hackrfFound = hack;
    emit devicesUpdated(rtlFound, hackrfFound);
  }
}
