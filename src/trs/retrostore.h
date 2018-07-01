
#ifndef __RETROSTORE_H__
#define __RETROSTORE_H__

#include "inout.h"
#include "key.h"
#include "window.h"
#include "form.h"
#include "header.h"
#include "list.h"
#include "menu.h"
#include "wifi.h"
#include "browse.h"
#include "about.h"
#include "help.h"
#include "panic.h"
#include "esp.h"
#include "version.h"

#define RS_PORT 31

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

#endif
