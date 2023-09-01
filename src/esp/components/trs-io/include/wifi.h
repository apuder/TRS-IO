
#pragma once

#include "defs.h"
#include <string>

using namespace std;

#define RS_STATUS_WIFI_NOT_NEEDED 0
#define RS_STATUS_WIFI_CONNECTING 1
#define RS_STATUS_WIFI_CONNECTED 2
#define RS_STATUS_WIFI_NOT_CONNECTED 3
#define RS_STATUS_WIFI_NOT_CONFIGURED 4
#define RS_STATUS_NO_RETROSTORE_CARD 0xff


void set_wifi_credentials(const char* ssid, const char* passwd);
uint8_t* get_wifi_status();
const char* get_wifi_ip();
void trs_printer_write(const char ch);
uint8_t trs_printer_read();
void init_wifi();
