#ifndef _FLASH_H_
#define _FLASH_H_

#define FLASH_SECTOR_SIZE          0x1000 // 4k


unsigned char flashStatusRegisterRead(void);
void flashRead(unsigned long adr, unsigned char *bfr, int n);
void flashWrite(unsigned long adr, unsigned char *bfr, int n);
void flashSectorErase(unsigned long adr);
unsigned long flashReadMfdDevId(void);
char *flashMfdDevIdStr(unsigned long mfg_dev_id);

#endif
