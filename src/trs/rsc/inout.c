
#include "inout.h"

void out(uint8_t port, uint8_t val)
{
  __asm
    pop hl          ; ret
    pop bc          ; port->c, val->b
    push bc
    push hl
    out (c),b
  __endasm;
}

uint8_t in(uint8_t port)
{
  __asm
    pop hl          ; ret
    pop bc          ; port->c
    push bc
    push hl
    in  l,(c)
  __endasm;
}


