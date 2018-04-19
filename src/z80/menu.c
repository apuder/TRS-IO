
#include "retrostore.h"


static window_t wnd_sel;
static window_t wnd_desc;

void init_menu(menu_t* menu,  mn_get_title_t get_title,
	       mn_get_count_t get_count, mn_get_item_t get_item) {
  menu->get_title = get_title;
  menu->get_count = get_count;
  menu->get_item = get_item;
  menu->sel = 0;
  menu->start_idx = 0;
}

uint16_t menu(menu_t* menu)
{
  uint16_t i, count;
  show_splash_screen(menu->get_title());
  init_window(&wnd_sel, 0, 3, 2, 0);
  init_window(&wnd_desc, 3, 3, 0, 0);
  wnd_print(&wnd_sel, false, MENU_HIGHLIGHT);
  for (i = 0; i < menu->sel; i++) {
    wnd_scroll_down(&wnd_sel);
  }
  count = menu->get_count();
  for (i = 0; i < count; i++) {
    if (i == wnd_desc.h) {
      break;
    }
    wnd_print(&wnd_desc, true, menu->get_item(menu->start_idx + i));
    wnd_cr(&wnd_desc);
  }

  while (true) {
    char ch = get_key();
    switch(ch) {
    case KEY_BREAK:
    case KEY_LEFT:
    case KEY_CLEAR:
      return MENU_EXIT;
    case KEY_ENTER:
    case KEY_RIGHT:
      return menu->start_idx + menu->sel;
    case KEY_UP:
      if (menu->sel == 0 && menu->start_idx == 0) {
	continue;
      }
      if (menu->sel == 0) {
	wnd_scroll_down(&wnd_desc);
	menu->start_idx--;
	wnd_print(&wnd_desc, true, menu->get_item(menu->start_idx));
      } else {
	menu->sel--;
	wnd_scroll_up(&wnd_sel);
      }
      break;
    case KEY_DOWN:
      if (menu->start_idx + menu->sel == count - 1) {
	continue;
      }
      if (menu->sel == wnd_sel.h - 1) {
	wnd_scroll_up(&wnd_desc);
	menu->start_idx++;
	wnd_print(&wnd_desc, true, menu->get_item(menu->start_idx + menu->sel));
      } else {
	wnd_scroll_down(&wnd_sel);
	menu->sel++;
      }
      break;
    default:
      break;
    }
  }

  return MENU_EXIT;
}
