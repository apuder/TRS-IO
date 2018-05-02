
#ifndef __MENU_H__
#define __MENU_H__

#include "window.h"

#define MENU_EXIT 0xff

#define MENU(name, title) \
  static menu_t name = { \
    title, \
    sizeof(name ## _items) / sizeof(menu_item_t), \
    name ## _items \
  }
  
typedef struct {
  uint8_t id;
  const char* option;
} menu_item_t;

typedef struct {
  const char* title;
  uint8_t num;
  menu_item_t* items;
} menu_t;

uint8_t menu(window_t* wnd, menu_t* menu, bool show_from_left);


#endif
