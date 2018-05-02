
#ifndef __LIST_H__
#define __LIST_H__

#include "window.h"

#define LIST_EXIT ((uint16_t) 0xffff)

#define LIST_HIGHLIGHT1 "->"
#define LIST_HIGHLIGHT2 "\214\235"
#define LIST_HIGHLIGHT LIST_HIGHLIGHT1

typedef const char* (*list_get_title_t)();
typedef uint16_t (*list_get_count_t)();
typedef const char* (*list_get_item_t)(uint16_t);

typedef struct {
  list_get_title_t get_title;
  list_get_count_t get_count;
  list_get_item_t get_item;
  uint8_t sel;
  uint16_t start_idx;
} list_t;

void init_list(list_t* menu,  list_get_title_t get_title,
	       list_get_count_t get_count, list_get_item_t get_item);
uint16_t list(window_t* wnd, list_t* list, bool show_from_left);

#endif
