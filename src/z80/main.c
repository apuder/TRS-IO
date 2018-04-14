
typedef unsigned char uint8_t;

extern unsigned char console_font_5x8[];

#define FONT_WIDTH 5
#define FONT_HEIGHT 8
#define FONT_HEIGHT_OFFSET 2

static void setPixel(uint8_t x, uint8_t y)
{
    __asm
        pop hl          ; ret
        pop bc          ; x->c, y->b
        push bc
        push hl
	ld a,b
	ld b,c
	ld h,#0x80
pixel:  push hl
	push bc
	ld hl,#dummy
	jp 0x0150
dummy:   .ascii    ');'
    __endasm;
}

void cls() {
  __asm
    jp 0x01c9
  __endasm;
}

void drawText(uint8_t x, uint8_t y, char* txt) {
  int i, j;
  char b;

  while (*txt != '\0') {
    unsigned int idx = *txt++ * FONT_HEIGHT + FONT_HEIGHT_OFFSET;
    for (i = 0; i < FONT_HEIGHT - FONT_HEIGHT_OFFSET; i++) {
      b = console_font_5x8[idx++];
      for (j = 0; j < FONT_WIDTH; j++) {
	if (b & (1 << (7 - j))) {
	  setPixel(x + j, y + i);
	}
      }
    }
    x += FONT_WIDTH + 1;
  }
}

void drawHorizontalLine(uint8_t x0, uint8_t x1, uint8_t y) {
  int i;
  for (i = x0; i < x1; i++) {
    setPixel(i, y);
  }
}

void showSplashScreen() {
  cls();
  drawText(0, 0, "RetroStore");
  drawHorizontalLine(0, 128, FONT_HEIGHT - FONT_HEIGHT_OFFSET + 1);
}

void main()
{
  showSplashScreen();
  while (1) ;
}

