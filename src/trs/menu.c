
#include "menu.h"
#include "list.h"

static menu_t* the_menu;
static list_t list_menu;

static const char* menu_get_title() {
  return the_menu->title;
}

static uint16_t menu_get_count() {
  return the_menu->num;
}

static const char* menu_get_item(uint16_t idx) {
  return the_menu->items[idx].option;
}


uint8_t menu(window_t* wnd, menu_t* menu, bool show_from_left) {
  uint16_t idx;
  the_menu = menu;
  init_list(&list_menu, menu_get_title, menu_get_count, menu_get_item);
  idx = list(wnd, &list_menu, show_from_left);
  return (idx == LIST_EXIT) ? MENU_EXIT : menu->items[idx].id;
}
