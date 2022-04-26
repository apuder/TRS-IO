
#pragma once

#include "defs.h"

#define RS_STATUS_WIFI_NOT_NEEDED 0
#define RS_STATUS_WIFI_CONNECTING 1
#define RS_STATUS_WIFI_CONNECTED 2
#define RS_STATUS_WIFI_NOT_CONNECTED 3
#define RS_STATUS_WIFI_NOT_CONFIGURED 4
#define RS_STATUS_NO_RETROSTORE_CARD 0xff

#define MAX_LEN_SSID 32
#define MAX_LEN_PASSWD 32
#define MAX_LEN_TZ 32
#define MAX_LEN_SMB_URL 100
#define MAX_LEN_SMB_USER 32
#define MAX_LEN_SMB_PASSWD 32

typedef struct {
  char ssid[MAX_LEN_SSID + 1];
  char passwd[MAX_LEN_PASSWD + 1];
  char tz[MAX_LEN_TZ + 1];
  char smb_url[MAX_LEN_SMB_URL + 1];
  char smb_user[MAX_LEN_SMB_USER + 1];
  char smb_passwd[MAX_LEN_SMB_PASSWD + 1];
} trs_io_wifi_config_t;

void set_wifi_credentials(const char* ssid, const char* passwd);
uint8_t* get_wifi_status();
const char* get_wifi_ssid();
const char* get_wifi_ip();
trs_io_wifi_config_t* get_wifi_config();
void trs_printer_write(const char ch);
uint8_t trs_printer_read();
void init_wifi();
