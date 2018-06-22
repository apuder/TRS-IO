
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
#include "panic.h"

#define RS_PORT 31

#define RS_SEND_BOOT 0
#define RS_SEND_CMD 1
#define RS_SEND_APP_TITLE 2
#define RS_SEND_APP_DETAILS 3
#define RS_SEND_STATUS 4
#define RS_CMD_CONFIGURE_WIFI 5
#define RS_CMD_SET_QUERY 6

#define RS_STATUS_WIFI_NOT_NEEDED 0
#define RS_STATUS_WIFI_CONNECTING 1
#define RS_STATUS_WIFI_CONNECTED 2
#define RS_STATUS_WIFI_NOT_CONNECTED 3
#define RS_STATUS_NO_RETROSTORE_CARD 0xff

#endif
