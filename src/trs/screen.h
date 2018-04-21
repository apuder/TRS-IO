
#ifndef __SCREEN_H__
#define __SCREEN_H__

#include "defs.h"
#include "window.h"
#include "key.h"
#include "panic.h"

#define SCREEN_TYPE_END 0
#define SCREEN_TYPE_TEXT 1
#define SCREEN_TYPE_INPUT 2

#define SCREEN_CURSOR "\260"

typedef struct {
  uint8_t len;
  const char* buf;
} screen_input_t;

typedef struct {
  uint8_t type;
  int8_t x, y;
  union {
    const char* text;
    screen_input_t input;
  } u;
} screen_t;

char show_screen(screen_t* screen, window_t* wnd);

#endif
