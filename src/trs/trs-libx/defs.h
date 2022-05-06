
#ifndef __DEFS_H__
#define __DEFS_H__

#include <string.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM

#include <inttypes.h>
#include <stdbool.h>

#else

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed int int16_t;
typedef unsigned int uint16_t;
typedef uint8_t bool;

#define true 1
#define false 0

#endif

#endif
