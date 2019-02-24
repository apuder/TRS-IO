
#include "defs.h"

#define PEEK_W(addr) (uint8_t*) *((uint16_t*) (addr))
#define POKE_W(addr, val) *((uint16_t*) addr) = (uint16_t) (val)

#define ORG 0xfa00

#define BAS_BEGIN 0x40a4
#define BAS_END 0x40f9
#define ARRAY_START 0x40fb
#define MEM_START 0x40fd

#define TRS_IO_RETROSTORE_MODULE_ID 3
#define TRS_IO_PORT 31

#define RS_SEND_BASIC 2

#define COMMUNICATIONS_REGION 0x4000

#include "mem-dump-4000-4200.c"

static void out(uint8_t port, uint8_t val)
{
  __asm
    pop hl          ; ret
    pop bc          ; port->c, val->b
    push bc
    push hl
    out (c),b
  __endasm;
}

static uint8_t in(uint8_t port)
{
  __asm
    pop hl          ; ret
    pop bc          ; port->c
    push bc
    push hl
    in  l,(c)
  __endasm;
}

static void wait_for_esp()
{
  while (in(0xe0) & 8) ;
}


static void init_ram()
{
  int i;

  // Copy communication region
  uint8_t* p = (uint8_t*) COMMUNICATIONS_REGION;
  for (i = 0; i < mem_dump_4000_4200_len; i++) {
    *p++ = mem_dump_4000_4200[i];
  }

  // Zero out rest of RAM
  for (i = 0; i < ORG - 0x4200; i++) {
    *p++ = 0;
  }
}

static void run()
{
  __asm
    ; Turn off cursor
    ld hl,#0x4022
    ld (hl),#0
    ; Copy 'RUN' to input buffer
    ld hl,(0x40a7)
    push hl
    ld (hl),#'R'
    inc hl
    ld (hl),#'U'
    inc hl
    ld (hl),#'N'
    inc hl
    ld (hl),#0
    pop hl
    dec hl
    ; Enter interpreter loop
    jp 0x1a81
  __endasm;
}

static void di()
{
  __asm
    di
  __endasm;
}

void main()
{
  uint8_t* last_line;
  uint8_t* p;
  uint8_t b;
  uint16_t w;

  di();
  init_ram();
  out(TRS_IO_PORT, TRS_IO_RETROSTORE_MODULE_ID);
  out(TRS_IO_PORT, RS_SEND_BASIC);
  wait_for_esp();
  // Ignore blob length
  in(TRS_IO_PORT);
  in(TRS_IO_PORT);
  b = in(TRS_IO_PORT);
  while (b != 0xff) ;
  last_line = PEEK_W(BAS_BEGIN);
  p = last_line;
  while (1) {
    w = in(TRS_IO_PORT) | (in(TRS_IO_PORT) << 8);
    if (w == 0) {
      break;
    }
    p++;
    p++;
    *p++ = in(TRS_IO_PORT);
    *p++ = in(TRS_IO_PORT);
    do {
      b = in(TRS_IO_PORT);
      *p++ = b;
    } while (b != 0);
    POKE_W(last_line, p);
    last_line = p;
  }
  *last_line++ = 0;
  *last_line++ = 0;
  POKE_W(BAS_END, last_line);
  POKE_W(ARRAY_START, last_line);
  POKE_W(MEM_START, last_line);
  run();
}
