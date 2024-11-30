#include "spi.h"
#include "xspi.h"
#include "flash.h"


#define FLASH_PAGE_PROGRAM         0x02
#define FLASH_READ                 0x03
#define FLASH_STATUS_REISTER_READ  0x05
#define FLASH_WRITE_ENABLE         0x06
#define FLASH_SECTOR_ERASE         0x20
#define FLASH_READ_MFG_DEV_ID      0x9f

#define FLASH_WIP                  0x01
#define FLASH_SECTOR_SIZE          0x1000 // 4k

/* Interface defines */
#define FLASH_CS    0x80
#define FLASH_WPN   0x40


static void flashWriteEnable(void)
{
   unsigned char cmd[1] = { FLASH_WRITE_ENABLE, };

   spi_set_spi_ctrl_reg(FLASH_CS | FLASH_WPN);
   spiSend(cmd, 1);
   spi_set_spi_ctrl_reg(FLASH_WPN);
}

unsigned char flashStatusRegisterRead(void)
{
   unsigned char cmd_dta[2] = { FLASH_STATUS_REISTER_READ, 0, };

   spi_set_spi_ctrl_reg(FLASH_CS | FLASH_WPN);
   spiExch(cmd_dta, 2);
   spi_set_spi_ctrl_reg(FLASH_WPN);

   return cmd_dta[1];
}

void flashRead(unsigned long adr, unsigned char *bfr, int n)
{
   unsigned char cmd_adr[4] =
   {
      FLASH_READ, (uint8_t) ((adr >> 16) & 0xff), (uint8_t) ((adr >> 8) & 0xff), (uint8_t) (adr & 0xff),
   };
   spi_set_spi_ctrl_reg(FLASH_CS | FLASH_WPN);
   spiSend(cmd_adr, 4);
   spiRecv(bfr, n);
   spi_set_spi_ctrl_reg(FLASH_WPN);
}

void flashWrite(unsigned long adr, unsigned char *bfr, int n)
{

   unsigned char cmd_adr[4] =
   {
      FLASH_PAGE_PROGRAM,  (uint8_t) ((adr >> 16) & 0xff), (uint8_t) ((adr >> 8) & 0xff), (uint8_t) (adr & 0xff),
   };

   flashWriteEnable();

   spi_set_spi_ctrl_reg(FLASH_CS | FLASH_WPN);
   spiSend(cmd_adr, 4);
   spiSend(bfr, n);
   spi_set_spi_ctrl_reg(FLASH_WPN);

   while(flashStatusRegisterRead() & FLASH_WIP)
      ;
}

void flashSectorErase(unsigned long adr)
{

   unsigned char cmd_adr[4] =
   {
      FLASH_SECTOR_ERASE,  (uint8_t) ((adr >> 16) & 0xff), (uint8_t) ((adr >> 8) & 0xff), (uint8_t) (adr & 0xff),
   };

   flashWriteEnable();

   spi_set_spi_ctrl_reg(FLASH_CS | FLASH_WPN);
   spiSend(cmd_adr, 4);
   spi_set_spi_ctrl_reg(FLASH_WPN);

   while(flashStatusRegisterRead() & FLASH_WIP)
      ;
}

unsigned long flashReadMfdDevId(void)
{
   unsigned char cmd_dta[4] =
   {
      FLASH_READ_MFG_DEV_ID, 0, 0, 0,
   };

   spi_set_spi_ctrl_reg(FLASH_CS | FLASH_WPN);
   spiExch(cmd_dta, 4);
   spi_set_spi_ctrl_reg(FLASH_WPN);

   return ((unsigned long)cmd_dta[1] << 16) | (cmd_dta[2] << 8) | cmd_dta[3];
}

char *flashMfdDevIdStr(unsigned long mfg_dev_id)
{
   unsigned char mfg_id = mfg_dev_id >> 16;
   unsigned short dev_id = mfg_dev_id;

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
   else if(mfg_id == 0x0b)
   {
      if(dev_id == 0x4016)
         return "XTX XT25F32B";
      else if(dev_id == 0x4017)
         return "XTX XT25F64";
      else
         return "XTX ?";
   }
   else
      return (char *)0;
}
