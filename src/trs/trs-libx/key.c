
#include "key.h"

static char dummy_get_key()
{
  return 0;
}

static get_key_t get_key_cb = dummy_get_key;

void set_keyboard_callback(get_key_t get_key)
{
  get_key_cb = get_key;
}

#ifdef ESP_PLATFORM
char get_key()
{
  return get_key_cb();
}

#else

char get_key() {
  __asm
    push de
    ld a,(0x125)
    sub #'I'
    jr z,rom_key ; This is a MIII
    ld a,#201    ; Enable BREAK
    ld (16396),a
rom_key:
    call 0x0049
    pop de
    ld l,a
  __endasm;
}

#endif
