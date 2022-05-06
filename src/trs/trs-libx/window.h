
#ifndef __WINDOW_H__
#define __WINDOW_H__

#include "defs.h"

typedef struct {
  uint8_t x;
  uint8_t y;
  uint8_t w;
  uint8_t h;
  uint8_t cx;
  uint8_t cy;
} window_t;

void init_window(window_t* wnd, uint8_t x, uint8_t y, int8_t w, int8_t h);
void wnd_print(window_t* wnd, bool single_line, const char* str);
void wnd_print_int(window_t* wnd, uint16_t v);
void wnd_cls(window_t* wnd);
void wnd_clear_eol(window_t* wnd);
void wnd_scroll_down(window_t* wnd);
void wnd_scroll_up(window_t* wnd);
void wnd_cr(window_t* wnd);
void wnd_goto(window_t* wnd, uint8_t x, uint8_t y);
void wnd_popup(const char* msg);

#endif
