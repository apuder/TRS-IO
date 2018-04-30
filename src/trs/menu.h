
#ifndef __MENU_H__
#define __MENU_H__

#include "window.h"

#define MENU_EXIT ((uint16_t) 0xffff)

#define MENU_HIGHLIGHT1 "->"
#define MENU_HIGHLIGHT2 "\214\235"
#define MENU_HIGHLIGHT MENU_HIGHLIGHT2

typedef const char* (*mn_get_title_t)();
typedef uint16_t (*mn_get_count_t)();
typedef const char* (*mn_get_item_t)(uint16_t);

typedef struct {
  mn_get_title_t get_title;
  mn_get_count_t get_count;
  mn_get_item_t get_item;
  uint8_t sel;
  uint16_t start_idx;
} menu_t;

void init_menu(menu_t* menu,  mn_get_title_t get_title,
	       mn_get_count_t get_count, mn_get_item_t get_item);
uint16_t menu(window_t* wnd, menu_t* menu, bool show_from_left);

#endif
