
#ifndef __RETROSTORE_H__
#define __RETROSTORE_H__

#include "defs.h"

#define RS_PORT 31

#define RS_STATE_READY 0
#define RS_STATE_PARSE_PARAMS 1
#define RS_STATE_NEED_STRING 2
#define RS_STATE_NEED_2B 3
#define RS_STATE_NEED_1B 4
#define RS_STATE_SEND 5

#define RS_SEND_BOOT 0
#define RS_SEND_CMD 1
#define RS_SEND_APP_TITLE 2
#define RS_SEND_APP_DETAILS 3
#define RS_SEND_STATUS 4
#define RS_CMD_CONFIGURE_WIFI 5
#define RS_CMD_SET_QUERY 6
#define RS_SEND_VERSION 7
#define RS_SEND_WIFI_SSID 8
#define RS_SEND_WIFI_IP 9
#define RS_SEND_BASIC 10


int rs_z80_out(int value);
int rs_z80_in();
void rs_get_send_buffer(uint8_t** buf, int* size);
void set_wifi_credentials(const char* ssid, const char* passwd);

#endif
