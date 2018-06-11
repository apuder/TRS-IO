
#include "header.h"
#include "form.h"
#include <string.h>

static window_t wnd_form;

typedef struct {
  uint8_t x, y;
  uint8_t len;
  uint8_t idx;
  char* buf;
} input_t;

#define MAX_INPUT_FIELDS 5

static input_t inputs[MAX_INPUT_FIELDS];

static void draw_input_field(input_t* inp, bool show_cursor) {
  int8_t i;
  wnd_goto(&wnd_form, inp->x, inp->y);
  wnd_print(&wnd_form, false, "[");
  wnd_print(&wnd_form, false, inp->buf);
  if (show_cursor) {
    wnd_print(&wnd_form, false, FORM_CURSOR);
  }
  for (i = inp->idx; i < inp->len - (show_cursor ? 1 : 0); i++) {
    wnd_print(&wnd_form, false, ".");
  }
  if (!show_cursor || inp->idx != inp->len) {
    wnd_print(&wnd_form, false, "]");
  }
}

static void redraw_input_fields(uint8_t n) {
  int i;
  for (i = 0; i < n; i++) {
    draw_input_field(&inputs[i], false);
  }
}

static char do_input(uint8_t n) {
  uint8_t i = 0;
  input_t* inp;
  char ch;

  while(true) {
    redraw_input_fields(n);
    inp = &inputs[i];
    while(true) {
      bool redraw = false;
      draw_input_field(inp, true);
      ch = get_key();
      switch (ch) {
      case KEY_BREAK:
        draw_input_field(inp, false);
	return ch;
      case KEY_LEFT:
	if (inp->idx == 0) {
	  continue;
	}
	inp->idx--;
	inp->buf[inp->idx] = '\0';
	break;
      case KEY_UP:
	i = (i - 1 + n) % n;
	redraw = true;
	break;
      case KEY_ENTER:
	if (i + 1 == n) {
          draw_input_field(inp, false);
	  return ch;
	}
	// Fallthrough
      case KEY_DOWN:
	i = (i + 1) % n;
	redraw = true;
	break;
      default:
	if (inp->idx == inp->len) {
	  continue;
	}
	inp->buf[inp->idx++] = ch;
	inp->buf[inp->idx] = '\0';
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

uint8_t form(window_t* wnd, const char* title, form_t* form,
             bool show_from_left) {
  char ch;
  int i;
  uint8_t num_input_fields = 0;

  wnd_switch_to_background(wnd);
  header(wnd, title);
  init_window(&wnd_form, 0, 3, 0, 0);
  
  while (form->type != FORM_TYPE_END) {
    if (form->x != -1 && form->y != -1) {
      wnd_goto(&wnd_form, form->x, form->y);
    }
    switch (form->type) {
    case FORM_TYPE_TEXT:
      wnd_print(&wnd_form, false, form->u.text);
      break;
    case FORM_TYPE_INPUT:
      if (num_input_fields == MAX_INPUT_FIELDS) {
	panic(ERR_TOO_MANY_INPUT_FIELDS);
      }
      i = num_input_fields++;
      inputs[i].len = form->u.input.len;
      inputs[i].buf = form->u.input.buf;
      inputs[i].x = wnd_form.cx;
      inputs[i].y = wnd_form.cy;
      inputs[i].idx = strlen(inputs[i].buf);
      break;
    default:
      panic(ERR_BAD_FORM_TYPE);
    }
    form++;
  }

  redraw_input_fields(num_input_fields);

  wnd_show(wnd, show_from_left);
  wnd_form.buffer = wnd->buffer;
  
  if (num_input_fields != 0) {
    return do_input(num_input_fields);
  }

  do {
    ch = get_key();
  } while ((ch != KEY_ENTER) && (ch != KEY_BREAK));

  return (ch == KEY_ENTER) ? FORM_OK : FORM_ABORT;
}
