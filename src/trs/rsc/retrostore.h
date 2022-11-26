
#ifndef __RETROSTORE_H__
#define __RETROSTORE_H__

#include "inout.h"
#include "wifi.h"
#include "browse.h"
#include "status.h"
#include "help.h"
#include "esp.h"
#include "version.h"


#define TRS_IO_PORT 31

#define TRS_IO_CORE_MODULE_ID 0

#define TRS_IO_SEND_VERSION 0
#define TRS_IO_SEND_STATUS 1
#define TRS_IO_CMD_CONFIGURE_WIFI 2
#define TRS_IO_SEND_WIFI_SSID 3
#define TRS_IO_SEND_WIFI_IP 4
#define TRS_IO_LOAD_XRAY_STATE 6


#define RETROSTORE_MODULE_ID 3

#define RS_SEND_VERSION 0
#define RS_SEND_LOADER_CMD 1
#define RS_SEND_BASIC 2
#define RS_SEND_CMD 3
#define RS_SEND_APP_TITLE 4
#define RS_SEND_APP_DETAILS 5
#define RS_CMD_SET_QUERY 6

#endif
