#include "spi.h"

/* Interface defines */
__sfr __at 0xfd SPI_DTA;


#if 1
void spiExch(unsigned char *bfr, int n) __naked
{
#if __SDCC_REVISION >= 12608
// New ABI
__asm
        ld      b,e
        ld      c,#0xfd
_exch:
        ld      a,(hl)
        out     (c),a
        ini
        jr      nz,_exch
        ret
__endasm;
#else
// Old ABI
__asm
        ld      iy,#2
        add     iy,sp
        ld      l,0(iy)
        ld      h,1(iy)
        ld      b,2(iy)
        ld      c,#0xfd
_exch:
        ld      a,(hl)
        out     (c),a
        ini
        jr      nz,_exch
        ret
__endasm;
#endif
}
#else
void spiExch(unsigned char *bfr, int n)
{
   while(n-- > 0)
   {
      SPI_DTA = *bfr;
      *bfr++ = SPI_DTA;
   }
}
#endif

#if 1
void spiSend(unsigned char *bfr, int n) __naked
{
#if __SDCC_REVISION >= 12608
// New ABI
__asm
        ld      b,e
        ld      c,#0xfd
        otir
        ret
__endasm;
#else
// Old ABI
__asm
        ld      iy,#2
        add     iy,sp
        ld      l,0(iy)
        ld      h,1(iy)
        ld      b,2(iy)
        ld      c,#0xfd
        otir
        ret
__endasm;
#endif
}
#else
void spiSend(unsigned char *bfr, int n)
{
   while(n-- > 0)
   {
      SPI_DTA = *bfr++;
   }
}
#endif

#if 1
void spiRecv(unsigned char *bfr, int n) __naked
{
#if __SDCC_REVISION >= 12608
// New ABI
__asm
        ld      b,e
        ld      c,#0xfd
        xor     a
_recv:
        out     (c),a
        ini
        jr      nz,_recv
        ret
__endasm;
#else
// Old ABI
__asm
        ld      iy,#2
        add     iy,sp
        ld      l,0(iy)
        ld      h,1(iy)
        ld      b,2(iy)
        ld      c,#0xfd
        xor     a
_recv:
        out     (c),a
        ini
        jr      nz,_recv
        ret
__endasm;
#endif
}
#else
void spiRecv(unsigned char *bfr, int n)
{
   while(n-- > 0)
   {
      SPI_DTA = 0;
      *bfr++ = SPI_DTA;
   }
}
#endif
