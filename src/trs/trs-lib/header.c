
#include "header.h"
#include "screen.h"

extern const unsigned char console_font_5x8[];

#define FONT_WIDTH 5
#define FONT_HEIGHT 8
#define FONT_HEIGHT_OFFSET 2

static void set_pixel(uint8_t x, uint8_t y) {
  uint8_t b, ox, oy;
  uint8_t* p;

  p = screen.current + (y / 3) * 64 + (x / 2);
  b = *p;
  if (b < 128 || b > 191) {
    b = 128;
  }
  ox = x % 2;
  oy = y % 3;
  b |= 1 << (ox + oy * 2);
  *p = b;
}

static void cls() {
  uint8_t* p = screen.current;
  int i;

  for (i = 0; i < screen.width * screen.height; i++) {
    *p++ = ' ';
  }
  screen_update_full();
}
  
static void draw_text(uint8_t x, uint8_t y, char* txt) {
  int i, j;
  char b;

  while (*txt != '\0') {
    unsigned int idx = *txt++ * FONT_HEIGHT + FONT_HEIGHT_OFFSET;
    for (i = 0; i < FONT_HEIGHT - FONT_HEIGHT_OFFSET; i++) {
      b = console_font_5x8[idx++];
      for (j = 0; j < FONT_WIDTH; j++) {
	if (b & (1 << (7 - j))) {
	  set_pixel(x + j, y + i);
	}
      }
    }
    x += FONT_WIDTH + 1;
  }
}

static void draw_horizontal_line(uint8_t x0, uint8_t x1,
                                 uint8_t y) {
  int i;
  for (i = x0; i < x1; i++) {
    set_pixel(i, y);
  }
}

void header(const char* banner) {
  cls();
  draw_text(0, 0, banner);
  draw_horizontal_line(0, screen.width * 2,
			 FONT_HEIGHT - FONT_HEIGHT_OFFSET + 1);
  screen_update_full();
}
