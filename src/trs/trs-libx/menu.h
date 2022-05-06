
#ifndef __MENU_H__
#define __MENU_H__

#include "window.h"
#include "list.h"

#define MENU_ABORT 0xff

#define MENU(name, title) \
  static menu_t name = { \
    title, \
    sizeof(name ## _items) / sizeof(menu_item_t), \
    name ## _items, \
    false \
  }
  
typedef struct {
  uint8_t id;
  const char* option;
} menu_item_t;

typedef struct {
  const char* title;
  uint8_t num;
  menu_item_t* items;
  bool list_initialized;
  list_t list;
} menu_t;

uint8_t menu(menu_t* menu, bool show_from_left, bool can_abort);


#endif
