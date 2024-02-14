#include "spi.h"
#include "xspi.h"


void spiExch(unsigned char *bfr, int n)
{
  while (n-- > 0) {
    spi_set_spi_data(*bfr);
    *bfr = spi_get_spi_data();
    bfr++;
  }
}

void spiSend(unsigned char *bfr, int n)
{
  while (n-- > 0) {
    spi_set_spi_data(*bfr++);
  }
}

void spiRecv(unsigned char *bfr, int n)
{
   while(n-- > 0) {
      *bfr++ = spi_get_spi_data();
   }
}
