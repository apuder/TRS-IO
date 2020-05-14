
#ifndef __LIST_H__
#define __LIST_H__

#include "window.h"

#define LIST_ABORT ((uint16_t) 0xffff)

#define LIST_HIGHLIGHT1 "->"
#define LIST_HIGHLIGHT2 "\214\235"
#define LIST_HIGHLIGHT LIST_HIGHLIGHT1

typedef bool (*list_get_item_t)(uint16_t, const char** name);

typedef struct {
  const char* title;
  list_get_item_t get_item;
  uint8_t sel;
  uint16_t start_idx;
} list_t;

void init_list(list_t* menu, const char* title, list_get_item_t get_item);
uint16_t list(list_t* list, bool show_from_left, bool can_abort);

#endif
