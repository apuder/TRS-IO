#include "spi.h"
#include "flash.h"
#include <trs-lib.h>


#define FLASH_PAGE_PROGRAM         0x02
#define FLASH_READ                 0x03
#define FLASH_STATUS_REISTER_READ  0x05
#define FLASH_WRITE_ENABLE         0x06
#define FLASH_SECTOR_ERASE         0x20
#define FLASH_READ_MFG_DEV_ID      0x9f

#define FLASH_WIP                  0x01
#define FLASH_SECTOR_SIZE          0x1000 // 4k

/* Interface defines */
__sfr __at 0xfc FLASH_CTL;
#define FLASH_CS    0x80
#define FLASH_WPN   0x40


static void flashWriteEnable(void)
{
   unsigned char cmd[1] = { FLASH_WRITE_ENABLE, };

   FLASH_CTL = FLASH_CS | FLASH_WPN;
   spiSend(cmd, 1);
   FLASH_CTL = FLASH_WPN;
}

unsigned char flashStatusRegisterRead(void)
{
   unsigned char cmd_dta[2] = { FLASH_STATUS_REISTER_READ, 0, };

   FLASH_CTL = FLASH_CS | FLASH_WPN;
   spiExch(cmd_dta, 2);
   FLASH_CTL = FLASH_WPN;

   return cmd_dta[1];
}

void flashRead(unsigned long adr, unsigned char *bfr, int n)
{
   unsigned char cmd_adr[4] =
   {
      FLASH_READ, adr >> 16, adr >> 8, adr,
   };
   FLASH_CTL = FLASH_CS | FLASH_WPN;
   spiSend(cmd_adr, 4);
   spiRecv(bfr, n);
   FLASH_CTL = FLASH_WPN;
}

void flashWrite(unsigned long adr, unsigned char *bfr, int n)
{

   unsigned char cmd_adr[4] =
   {
      FLASH_PAGE_PROGRAM, adr >> 16, adr >> 8, adr,
   };

   flashWriteEnable();

   FLASH_CTL = FLASH_CS | FLASH_WPN;
   spiSend(cmd_adr, 4);
   spiSend(bfr, n);
   FLASH_CTL = FLASH_WPN;

   while(flashStatusRegisterRead() & FLASH_WIP)
      ;
}

void flashSectorErase(unsigned long adr)
{

   unsigned char cmd_adr[4] =
   {
      FLASH_SECTOR_ERASE, adr >> 16, adr >> 8, adr,
   };

   flashWriteEnable();

   FLASH_CTL = FLASH_CS | FLASH_WPN;
   spiSend(cmd_adr, 4);
   FLASH_CTL = FLASH_WPN;

   while(flashStatusRegisterRead() & FLASH_WIP)
      ;
}

unsigned long flashReadMfdDevId(void)
{
   unsigned char cmd_dta[4] =
   {
      FLASH_READ_MFG_DEV_ID, 0, 0, 0,
   };

   FLASH_CTL = FLASH_CS | FLASH_WPN;
   spiExch(cmd_dta, 4);
   FLASH_CTL = FLASH_WPN;

   return ((unsigned long)cmd_dta[1] << 16) | (cmd_dta[2] << 8) | cmd_dta[3];
}

char *flashMfdDevIdStr(unsigned long mfg_dev_id)
{
   unsigned char mfg_id = mfg_dev_id >> 16;
   unsigned int  dev_id = mfg_dev_id;

   if(mfg_id == 0xc2)
   {
      if(dev_id == 0x2016)
         return "Macronix MX25L3233F";
      else
         return "Macronix ?";
   }
   else if(mfg_id == 0x20)
   {
      if(dev_id == 0xba16)
         return "Micron N25Q032A";
      else
         return "Micron ?";
   }
   else
      return (char *)0;
}
