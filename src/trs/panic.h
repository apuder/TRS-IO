
#ifndef __PANIC_H__
#define __PANIC_H__

#include "defs.h"

#define ERR_BAD_SCREEN_TYPE 0
#define ERR_TOO_MANY_INPUT_FIELDS 1

void panic(uint8_t errno);

#endif
