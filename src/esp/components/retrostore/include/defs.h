
#ifndef __DEFS_H__
#define __DEFS_H__

#ifdef ESP_PLATFORM
#include "esp_system.h"
#else

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
//typedef uint8_t bool;

#define true 1
#define false 0

#endif

#endif
