
#include <trs-io.h>
#include "esp_mock.h"
#include "version.h"


class TrsIOCoreModule : public TrsIO {
public:
  TrsIOCoreModule(int id) : TrsIO(id) {
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendVersion), "");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendStatus), "");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::configureWifi), "SS");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendWifiSSID), "");
    addCommand(static_cast<cmd_t>(&TrsIOCoreModule::sendWifiIP), "");
  }

  void sendVersion() {
    addByte(TRS_IO_HARDWARE_REVISION);
    addByte(TRS_IO_VERSION_MAJOR);
    addByte(TRS_IO_VERSION_MINOR);
  }

  void sendStatus() {
    addByte(*get_wifi_status());
  }

  void configureWifi() {
    const char* ssid = S(0);
    const char* passwd = S(1);
    set_wifi_credentials(ssid, passwd);
  }

  void sendWifiSSID() {
    const char* ssid = get_wifi_ssid();
    addStr(ssid);
  }

  void sendWifiIP() {
    const char* ip = get_wifi_ip();
    addStr(ip);
  }
};

TrsIOCoreModule theTrsIOCoreModule(TRS_IO_CORE_MODULE_ID);
