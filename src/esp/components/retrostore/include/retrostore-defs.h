
#ifndef __RETROSTORE_DEFS_H__
#define __RETROSTORE_DEFS_H__

#include "retrostore.h"
#include "defs.h"

#define RS_VERSION_MAJOR 2
#define RS_VERSION_MINOR 0

#define RETROSTORE_MODULE_ID 3

#define RS_SEND_VERSION 0
#define RS_SEND_LOADER_CMD 1
#define RS_SEND_BASIC 2
#define RS_SEND_CMD 3
#define RS_SEND_APP_TITLE 4
#define RS_SEND_APP_DETAILS 5
#define RS_CMD_SET_QUERY 6

extern retrostore::RetroStore rs;

#endif
