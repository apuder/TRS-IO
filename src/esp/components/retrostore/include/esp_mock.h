
#pragma once

#include "defs.h"

uint8_t* get_wifi_status();
void set_wifi_credentials(const char* ssid, const char* passwd);
const char* get_wifi_ssid();
const char* get_wifi_ip();
