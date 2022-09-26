
#include "retrostore.h"
#include "esp_mock.h"

#ifndef ESP_PLATFORM

uint8_t* get_wifi_status()
{
  // RS_STATUS_WIFI_CONNECTED
  static uint8_t status = 2;
  return &status;
}

void set_wifi_credentials(const char* ssid, const char* passwd)
{
  // Do nothing
}

const char* get_wifi_ssid()
{
  return "-";
}

const char* get_wifi_ip()
{
  return "-";
}

#endif
