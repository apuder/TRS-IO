
#ifndef __RETROSTORE_H__
#define __RETROSTORE_H__

#include "defs.h"

#define RS_PORT 31

#define RS_STATE_READY 0
#define RS_STATE_NEED_2B 1
#define RS_STATE_NEED_1B 2
#define RS_STATE_SEND 3

#define RS_SEND_BOOT 0
#define RS_SEND_CMD 1
#define RS_SEND_APP_TITLE 2

int rs_z80_out(int value);
int rs_z80_in();
void rs_get_send_buffer(uint8_t** buf, int* size);


#endif
