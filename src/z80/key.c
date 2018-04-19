
#include "key.h"

char get_key() {
  __asm
    push de
    call 0x0049
    pop de
    ld l,a
  __endasm;
}
