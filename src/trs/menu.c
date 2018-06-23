
#include "menu.h"
#include "list.h"

static menu_t* the_menu;
static list_t list_menu;

static bool menu_get_item(uint16_t idx, const char** name) {
  if (idx >= the_menu->num) {
    return false;
  }
  *name = the_menu->items[idx].option;
  return true;
}


uint8_t menu(window_t* wnd, menu_t* menu, bool show_from_left, bool can_abort)
{
  uint16_t idx;
  the_menu = menu;
  init_list(&list_menu, menu->title, menu_get_item);
  idx = list(wnd, &list_menu, show_from_left, can_abort);
  return (idx == LIST_EXIT) ? MENU_EXIT : menu->items[idx].id;
}
