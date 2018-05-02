
#include "retrostore.h"


static window_t wnd_sel;
static window_t wnd_desc;

void init_list(list_t* list,  list_get_title_t get_title,
	       list_get_count_t get_count, list_get_item_t get_item) {
  list->get_title = get_title;
  list->get_count = get_count;
  list->get_item = get_item;
  list->sel = 0;
  list->start_idx = 0;
}

uint16_t list(window_t* wnd, list_t* list, bool show_from_left)
{
  uint16_t i, count;
  wnd_switch_to_background(wnd);
  show_splash_screen(wnd, list->get_title());
  init_window(&wnd_sel, 0, 3, 2, 0);
  init_window(&wnd_desc, 3, 3, 0, 0);
  wnd_print(&wnd_sel, false, LIST_HIGHLIGHT);
  for (i = 0; i < list->sel; i++) {
    wnd_scroll_down(&wnd_sel);
  }
  count = list->get_count();
  for (i = 0; i < count; i++) {
    if (i == wnd_desc.h) {
      break;
    }
    wnd_print(&wnd_desc, true, list->get_item(list->start_idx + i));
    wnd_cr(&wnd_desc);
  }
  wnd_show(wnd, show_from_left);

  wnd_sel.buffer = wnd->buffer;
  wnd_desc.buffer = wnd->buffer;

  while (true) {
    char ch = get_key();
    switch(ch) {
    case KEY_BREAK:
    case KEY_LEFT:
    case KEY_CLEAR:
      return LIST_EXIT;
    case KEY_ENTER:
    case KEY_RIGHT:
      return list->start_idx + list->sel;
    case KEY_UP:
      if (list->sel == 0 && list->start_idx == 0) {
	continue;
      }
      if (list->sel == 0) {
	wnd_scroll_down(&wnd_desc);
	list->start_idx--;
	wnd_print(&wnd_desc, true, list->get_item(list->start_idx));
      } else {
	list->sel--;
	wnd_scroll_up(&wnd_sel);
      }
      break;
    case KEY_DOWN:
      if (list->start_idx + list->sel == count - 1) {
	continue;
      }
      if (list->sel == wnd_sel.h - 1) {
	wnd_scroll_up(&wnd_desc);
	list->start_idx++;
	wnd_print(&wnd_desc, true, list->get_item(list->start_idx + list->sel));
      } else {
	wnd_scroll_down(&wnd_sel);
	list->sel++;
      }
      break;
    default:
      break;
    }
  }

  return LIST_EXIT;
}
