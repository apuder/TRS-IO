
#include "window.h"

static const uint16_t screen_base = 0x3c00;
static const uint8_t screen_width = 64;
static const uint8_t screen_height = 16;

static const uint8_t scroll_increment = 5;

static uint8_t background_buffer[64 * 16];

inline uint8_t* get_screen_pos0(window_t* wnd, uint8_t x, uint8_t y) {
  return (wnd->buffer + (wnd->y + y) * screen_width +
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
  wnd->w = (w > 0) ? w : screen_width + w - x;
  wnd->h = (h > 0) ? h : screen_height + h - y;
  wnd_switch_to_background(wnd);
}

void wnd_switch_to_background(window_t* wnd) {
  wnd->buffer = (uint16_t) background_buffer;
}

static void wnd_show_from_right(window_t* wnd) {
  uint8_t x, y, cx;
  int8_t left;
  uint8_t* from;
  uint8_t* to;
  uint8_t* end;

  cx = 0;
  left = screen_width;

  while (left > 0) {
    uint8_t d = (left >= scroll_increment) ? scroll_increment : left;
    for (y = 0; y < screen_height; y++) {
      to = (uint8_t*) (screen_base + y * screen_width);
      from = to + d;
      end = to + (screen_width - d);
      while (to < end) {
        *to++ = *from++;
      }

      from = (uint8_t*) (wnd->buffer + y * screen_width + cx);
      for (x = 0; x < d; x++) {
        *to++ = *from++;
      }
    }
    cx += d;
    left -= scroll_increment;
  }
  wnd->buffer = screen_base;
}

static void wnd_show_from_left(window_t* wnd) {
  uint8_t x, y;
  int8_t left;
  uint8_t* from;
  uint8_t* to;
  uint8_t* end;

  left = screen_width;

  while (left > 0) {
    uint8_t d = (left >= scroll_increment) ? scroll_increment : left;
    for (y = 0; y < screen_height; y++) {
      to = (uint8_t*) (screen_base + (y + 1) * screen_width - 1);
      from = to - d;
      end = to - screen_width + d;
      while (to > end) {
        *to-- = *from--;
      }

      from = (uint8_t*) (wnd->buffer + y * screen_width + left - 1);
      for (x = 0; x < d; x++) {
        *to-- = *from--;
      }
    }
    left -= scroll_increment;
  }
  wnd->buffer = screen_base;
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
    uint8_t* from = to + screen_width;
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
    uint8_t* from = to - screen_width;
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
