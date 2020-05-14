
#include "header.h"
#include "form.h"
#include "screen.h"
#include <string.h>

static window_t wnd_form;
static uint8_t num_form_items;
static uint8_t form_items_col1;
static uint8_t form_items_col2;
static uint8_t form_items_col3;


static void draw_input_field(form_input_t* inp, bool has_focus) {
  int8_t i;
  uint8_t idx;
  uint8_t len;
  uint8_t width;

  len = strlen(inp->buf);
  width = inp->width;
  width = (width == 0) ? inp->len : width;

  if (!has_focus) {
    wnd_print(&wnd_form, false, " ");
    if (len == 0) {
      wnd_print(&wnd_form, false, "-");
    } else if (len <= width) {
      wnd_print(&wnd_form, false, inp->buf);
    } else {
      char buf[2] = {0, 0};
      for (i = 0; i < width - 3; i++) {
	buf[0] = inp->buf[i];
	wnd_print(&wnd_form, false, buf);
      }
      wnd_print(&wnd_form, false, "...");
    }
    wnd_clear_eol(&wnd_form);
    return;
  }
  
  wnd_print(&wnd_form, false, "[");
  idx = (len > width) ? len - width : 0;
  wnd_print(&wnd_form, false, inp->buf + idx);
  wnd_print(&wnd_form, false, FORM_CURSOR);
  for (i = len; i < width - 1; i++) {
    wnd_print(&wnd_form, false, ".");
  }
  if (len < width) {
    wnd_print(&wnd_form, false, "]");
  }
}

static void draw_checkbox_field(form_checkbox_t* cb, bool has_focus)
{
  wnd_print(&wnd_form, false, has_focus ? "[" : " ");
  wnd_print(&wnd_form, false, *cb->checked ? "Yes" : "No");
  wnd_print(&wnd_form, false, has_focus ? "] " : "  ");
}

static void draw_select_field_items(form_select_t* sel)
{
  uint8_t current = sel->first;

  do {
    char* item;
    bool frame;
    
    item = (*sel->items)[current];
    if (item == NULL) {
      current = 0;
      continue;
    }
    frame = current == *sel->selected;
    wnd_print(&wnd_form, false, frame ? "[" : " ");
    wnd_print(&wnd_form, false, item);
    wnd_print(&wnd_form, false, frame ? "] " : "  ");
    current++;
  } while (current != sel->first);
}

static void draw_select_field(form_select_t* sel, bool has_focus)
{
  if (!has_focus) {
    sel->first = *sel->selected;
    wnd_print(&wnd_form, false, " ");
    wnd_print(&wnd_form, false, (*sel->items)[*sel->selected]);
    wnd_clear_eol(&wnd_form);
    return;
  }
  draw_select_field_items(sel);
}

static void redraw_form_item(uint8_t n, form_item_t* item, bool has_focus)
{
  wnd_goto(&wnd_form, form_items_col3, n);
  switch (item->type) {
  case FORM_TYPE_INPUT:
    draw_input_field(&item->u.input, has_focus);
    break;
  case FORM_TYPE_CHECKBOX:
    draw_checkbox_field(&item->u.checkbox, has_focus);
    break;
  case FORM_TYPE_SELECT:
    draw_select_field(&item->u.select, has_focus);
    break;
  default:
    // Skip
    break;
  }
}

static void redraw_form(form_item_t* form) {
  int n = 0;

  while (form->type != FORM_TYPE_END) {
    redraw_form_item(n, form, false);
    form++;
    n++;
  }
}

static void handle_key_for_input(uint8_t n, char key, form_input_t* inp)
{
  uint8_t len = strlen(inp->buf);

  if (key == KEY_LEFT) {
    if (len == 0) {
      return;
    }
    len--;
    inp->buf[len] = '\0';
  } else {
    if (len == inp->len) {
      return;
    }
    inp->buf[len++] = key;
    inp->buf[len] = '\0';
  }
  wnd_goto(&wnd_form, form_items_col3, n);
  draw_input_field(inp, true);
}

static void handle_key_for_checkbox(uint8_t n, char key, form_checkbox_t* cb)
{
  bool checked = *cb->checked;
  
  if (key == ' ') {
    checked = !checked;
  }
  if (key == 'N' || key == 'n') {
    checked = false;
  }
  if (key == 'Y' || key == 'y') {
    checked = true;
  }
  *cb->checked = checked;
  wnd_goto(&wnd_form, form_items_col3, n);
  draw_checkbox_field(cb, true);
}

static void handle_key_for_select(uint8_t n, char key, form_select_t* sel)
{
  uint8_t selected = *sel->selected;
  
  if (key == ' ' || key == KEY_RIGHT) {
    selected++;
    if ((*sel->items)[selected] == NULL) {
      selected = 0;
    }
  }
  if (key == KEY_LEFT) {
    if (selected == 0) {
      while ((*sel->items)[++selected] != NULL) ;
    }
    selected--;
  }
  *sel->selected = selected;
  wnd_goto(&wnd_form, form_items_col3, n);
  draw_select_field(sel, true);
}

static void handle_key(uint8_t n, char key, form_item_t* item)
{
  switch(item->type) {
  case FORM_TYPE_INPUT:
    handle_key_for_input(n, key, &item->u.input);
    break;
  case FORM_TYPE_CHECKBOX:
    handle_key_for_checkbox(n, key, &item->u.checkbox);
    break;
  case FORM_TYPE_SELECT:
    handle_key_for_select(n, key, &item->u.select);
    break;
  }
}

static char do_input(form_item_t* form, bool show_from_left)
{
  uint8_t i = 0;
  form_item_t* field;
  bool first_time = true;
  char ch;

  while (form[i].type == FORM_TYPE_HEADER) i++;
  
  while(true) {
    redraw_form(form);
    field = &form[i];
    while(true) {
      bool redraw = false;
      redraw_form_item(i, field, true);
      if (first_time) {
	screen_show(show_from_left);
	set_screen_to_foreground();
	first_time = false;
      }
      ch = get_key();
      switch (ch) {
      case KEY_BREAK:
        redraw_form_item(i, field, false);
	return ch;
      case KEY_UP:
	do {
	  i = (i - 1 + num_form_items) % num_form_items;
	} while (form[i].type == FORM_TYPE_HEADER);
	redraw = true;
	break;
      case KEY_ENTER:
	if (i + 1 == num_form_items) {
          redraw_form_item(i, field, false);
	  return ch;
	}
	// Fallthrough
      case KEY_DOWN:
	do {
	  i = (i + 1) % num_form_items;
	} while (form[i].type == FORM_TYPE_HEADER);
	redraw = true;
	break;
      default:
	handle_key(i, ch, field);
	break;
      }
      if (redraw) {
	break;
      }
    }
  }
  // Never reached
  return 0;
}

static void compute_form_layout(form_item_t* form)
{
  bool has_header = false;
  int max = 0;

  num_form_items = 0;
  
  while (form->type != FORM_TYPE_END) {
    num_form_items++;
    if (form->type == FORM_TYPE_HEADER) {
      has_header = true;
    } else {
      int len = strlen(form->label);
      max = (max < len) ? len : max;
    }
    form++;
  }
  form_items_col1 = has_header ? 2 : 0;
  form_items_col3 = has_header ? max + 4 : max + 2;
  form_items_col2 = form_items_col3 - 2;
}

static void draw_form(form_item_t* form)
{
  uint8_t n = 0;
  uint8_t len;
  form_item_t* save = form;
  bool is_header;
  
  while (form->type != FORM_TYPE_END) {
    if (n == wnd_form.h) {
      panic(0); // XXX
    }
    is_header = form->type == FORM_TYPE_HEADER;
    wnd_goto(&wnd_form, is_header ? 0 : form_items_col1, n);
    wnd_print(&wnd_form, false, form->label);
    if (!is_header) {
      wnd_goto(&wnd_form, form_items_col2, n);
      wnd_print(&wnd_form, false, ":");
    }
    form++;
    n++;
  }
  redraw_form(save);
}    

uint8_t form(const char* title, form_item_t* form,
             bool show_from_left)
{
  set_screen_to_background();
  header(title);
  init_window(&wnd_form, 0, 3, 0, 0);
  compute_form_layout(form);
  draw_form(form);
  return do_input(form, show_from_left);
}
