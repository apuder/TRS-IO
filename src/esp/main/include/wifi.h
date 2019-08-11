
#pragma once

#include "defs.h"

#define RS_STATUS_WIFI_NOT_NEEDED 0
#define RS_STATUS_WIFI_CONNECTING 1
#define RS_STATUS_WIFI_CONNECTED 2
#define RS_STATUS_WIFI_NOT_CONNECTED 3
#define RS_STATUS_WIFI_NOT_CONFIGURED 4
#define RS_STATUS_NO_RETROSTORE_CARD 0xff

extern uint8_t* wifi_status;

void set_wifi_credentials(const char* ssid, const char* passwd);
void init_wifi();
