
#include "window.h"
#include <string.h>

static const uint8_t scroll_increment = 5;

static uint8_t background_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];

inline uint8_t* get_screen_pos0(window_t* wnd, uint8_t x, uint8_t y) {
  return (wnd->buffer + (wnd->y + y) * SCREEN_WIDTH +
		     (wnd->x + x));
}

inline uint8_t* get_screen_pos(window_t* wnd) {
  return get_screen_pos0(wnd, wnd->cx, wnd->cy);
}

void init_window(window_t* wnd, uint8_t x, uint8_t y, int8_t w, int8_t h) {
  wnd->cx = 0;
  wnd->cy = 0;
  wnd->x = x;
  wnd->y = y;
  wnd->w = (w > 0) ? w : SCREEN_WIDTH + w - x;
  wnd->h = (h > 0) ? h : SCREEN_HEIGHT + h - y;
  wnd_switch_to_background(wnd);
}

void wnd_switch_to_background(window_t* wnd) {
  wnd->buffer = (uint16_t) background_buffer;
}

void wnd_switch_to_foreground(window_t* wnd) {
  wnd->buffer = SCREEN_BASE;
}

static void wnd_show_from_right(window_t* wnd) {
  uint8_t y, cx;
  int8_t left;
  uint8_t* from;
  uint8_t* to;

  cx = 0;
  left = SCREEN_WIDTH;

  while (left > 0) {
    uint8_t d = (left >= scroll_increment) ? scroll_increment : left;
    for (y = 0; y < SCREEN_HEIGHT; y++) {
      to = (uint8_t*) (SCREEN_BASE + y * SCREEN_WIDTH);
      from = to + d;
      memmove(to, from, SCREEN_WIDTH - d);

      to = (uint8_t*) (SCREEN_BASE + (y + 1) * SCREEN_WIDTH - d);
      from = (uint8_t*) (wnd->buffer + y * SCREEN_WIDTH + cx);
      memmove(to, from, d);
    }
    cx += d;
    left -= scroll_increment;
  }
  wnd->buffer = SCREEN_BASE;
}

static void wnd_show_from_left(window_t* wnd) {
  uint8_t y;
  int8_t left;
  uint8_t* from;
  uint8_t* to;

  left = SCREEN_WIDTH;

  while (left > 0) {
    uint8_t d = (left >= scroll_increment) ? scroll_increment : left;
    for (y = 0; y < SCREEN_HEIGHT; y++) {
      from = (uint8_t*) (SCREEN_BASE + y * SCREEN_WIDTH);
      to = from + d;
      memmove(to, from, SCREEN_WIDTH - d);

      from = (uint8_t*) (wnd->buffer + y * SCREEN_WIDTH + left - d);
      to = (uint8_t*) (SCREEN_BASE + y * SCREEN_WIDTH);
      memmove(to, from, d);
    }
    left -= scroll_increment;
  }
  wnd->buffer = SCREEN_BASE;
}
      
void wnd_show(window_t* wnd, bool from_left) {
  if (from_left) {
    wnd_show_from_left(wnd);
  } else {
    wnd_show_from_right(wnd);
  }
}
    
void wnd_cr(window_t* wnd) {
  wnd->cx = 0;
  wnd->cy++;
}

void wnd_goto(window_t* wnd, uint8_t x, uint8_t y) {
  wnd->cx = x;
  wnd->cy = y;
}

void wnd_print(window_t* wnd, bool single_line, const char* str) {
  const char* start_of_word;
  uint8_t len;
  uint8_t* p;
  
  while (*str != '\0') {
    const char* next = str;

    if (*next == '\n') {
      if (wnd->cy + 1 == wnd->h) {
        // We are in the last line. Just exit
        return;
      }
      wnd->cx = 0;
      wnd->cy++;
      str++;
      continue;
    }

    while ((*next != '\0') && (*next == ' ')) {
      next++;
    }

    start_of_word = next;

    while ((*next != '\0') && (*next != ' ') && (*next != '\n')) {
      next++;
    }

    len = next - str;

    // If we are in the last line, add space for 3 for ellipses
    if (wnd->cy + 1 == wnd->h) {
      len += 3;
    }
    
    // Fits in current line?
    if (wnd->cx + len > wnd->w) {
      // No. Do we have another line?
      if (single_line || wnd->cy + 1 == wnd->h) {
	// No. Stop outputting and print "..."
	p = get_screen_pos(wnd);
	*p++ = '.';
	*p++ = '.';
	*p = '.';
	return;
      }
      // Yes. Continue on next line
      wnd->cy++;
      wnd->cx = 0;

      str = start_of_word;
    }

    p = get_screen_pos(wnd);
  
    while (str != next) {
      *p++ = *str++;
      wnd->cx++;
    }
  }
} 

void wnd_print_int(window_t* wnd, uint16_t v) {
  char buf[10];
  int i = sizeof(buf);

  buf[--i] = '\0';
  while (v != 0) {
    buf[--i] = v % 10 + '0';
    v = v / 10;
  }
  if (i == sizeof(buf) - 1) {
    buf[--i] = '0';
  }
  wnd_print(wnd, false, buf + i);
}

void wnd_cls(window_t* wnd) {
  uint8_t x, y;

  for (y = 0; y < wnd->h; y++) {
    uint8_t* p = get_screen_pos0(wnd, 0, y);

    for (x = 0; x < wnd->w; x++) {
      *p++ = ' ';
    }
  }
  wnd->cx = 0;
  wnd->cy = 0;
}  

void wnd_scroll_up(window_t* wnd) {
  int8_t x, y;
  int8_t* clear;

  for (y = 0; y < wnd->h - 1; y++) {
    uint8_t* to = get_screen_pos0(wnd, 0, y);
    uint8_t* from = to + SCREEN_WIDTH;
    clear = from;
    
    for (x = 0; x < wnd->w; x++) {
      *to++ = *from++;
    }
  }
 
  for (x = 0; x < wnd->w; x++) {
    *clear++ = ' ';
  }

  wnd->cx = 0;
  wnd->cy = wnd->h - 1;
}

void wnd_scroll_down(window_t* wnd) {
  int8_t x, y;
  uint8_t* clear;

  for (y = wnd->h - 1; y > 0; y--) {
    uint8_t* to = get_screen_pos0(wnd, 0, y);
    uint8_t* from = to - SCREEN_WIDTH;
    clear = from;
    
    for (x = 0; x < wnd->w; x++) {
      *to++ = *from++;
    }
  }
 
  for (x = 0; x < wnd->w; x++) {
    *clear++ = ' ';
  }

  wnd->cx = 0;
  wnd->cy = 0;
}

void wnd_popup(window_t* wnd, const char* msg) {
  int len = strlen(msg) + 2;
  int cx = (wnd->w - len) / 2;
  int cy = wnd->h / 2 - 1;
  uint8_t* p0 = get_screen_pos0(wnd, cx, cy);
  uint8_t* p1 = get_screen_pos0(wnd, cx, cy + 1);
  uint8_t* p2 = get_screen_pos0(wnd, cx, cy + 2);
  int x, y;
  
  for (x = 0; x < len; x++) {
    *p0++ = 176;
    *p2++ = 131;
    if (x == 0 || x == len - 1) {
      *p1 = 191;
    } else {
      *p1 = msg[x - 1];
    }
    p1++;
  }
}
