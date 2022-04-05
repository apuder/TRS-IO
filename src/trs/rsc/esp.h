
#ifndef __ESP_H__
#define __ESP_H__

#include "defs.h"

#define RS_STATUS_WIFI_NOT_NEEDED 0
#define RS_STATUS_WIFI_CONNECTING 1
#define RS_STATUS_WIFI_CONNECTED 2
#define RS_STATUS_WIFI_NOT_CONNECTED 3
#define RS_STATUS_WIFI_NOT_CONFIGURED 4
#define RS_STATUS_NO_RETROSTORE_CARD 0xff

void wait_for_esp();
uint8_t scan();
void get_trs_io_version(uint8_t* revision, uint16_t* version);
void get_retrostore_version(uint16_t* version);
bool has_xray_support();

#endif
