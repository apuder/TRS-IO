#ifndef _XSPI_H_
#define _XSPI_H_

void spiExch(unsigned char *bfr, int n);
void spiSend(unsigned char *bfr, int n);
void spiRecv(unsigned char *bfr, int n);

#endif
