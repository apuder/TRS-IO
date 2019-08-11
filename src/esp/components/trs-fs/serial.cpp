
#include "trs-fs.h"
#include "serial.h"

#include "driver/uart.h"
#include "driver/gpio.h"


#define SERIAL_IO_TXD  GPIO_NUM_1
#define SERIAL_IO_RXD  GPIO_NUM_3
#define SERIAL_IO_RTS  (UART_PIN_NO_CHANGE)
#define SERIAL_IO_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE 1024

static int16_t read_byte() {
  int len;
  uint8_t data;

  do {
    uart_get_buffered_data_len(UART_NUM_0, (size_t*)&len);
  } while (len < 1);
 
  uart_read_bytes(UART_NUM_0, &data, 1, 0);
  return data;
}

static void read_blob(void* buf, int32_t btr) {
  int len;
  
  do {
    uart_get_buffered_data_len(UART_NUM_0, (size_t*)&len);
  } while (len < btr);
 
  uart_read_bytes(UART_NUM_0, (uint8_t*) buf, btr, 0);
}

static void write_byte(uint8_t b) {
  uart_write_bytes(UART_NUM_0, (const char*) &b, 1);
}

static void write_blob(const void* buf, int32_t btw) {
  uart_write_bytes(UART_NUM_0, (const char*) buf, btw);
}

static FRESULT read_fresult() {
  return (FRESULT) read_byte();
}

static void init_serial_io() {
  uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };
  uart_param_config(UART_NUM_0, &uart_config);
  uart_set_pin(UART_NUM_0, SERIAL_IO_TXD, SERIAL_IO_RXD, SERIAL_IO_RTS, SERIAL_IO_CTS);
  uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
}


static uint16_t read_word() {
    return read_byte() | (read_byte() << 8);
}

static void write_word(uint16_t w) {
    write_byte(w & 0xff);
    write_byte((w >> 8) & 0xff);
}

static uint32_t read_long() {
    return read_word() | (read_word() << 16);
}

static void write_long(uint32_t l) {
    write_word(l & 0xffff);
    write_word((l >> 16) & 0xffff);
}

static void write_string(const char *str) {
    do {
        write_byte(*str);
    } while (*str++ != '\0');
}

static void read_string(char* str) {
    do {
        *str = read_byte();
    } while (*str++ != '\0');
}

static void send_marker() {
    static const char* MARKER = "TRS-IO:FS:";

    const char* p = MARKER;
    while (*p != '\0') {
        write_byte(*p++);
    }
}

TRS_FS_SERIAL::TRS_FS_SERIAL() {
  init_serial_io();
}
  
void TRS_FS_SERIAL::f_log(const char* msg) {
  send_marker();
  write_byte(F_LOG);
  write_string(msg);
  read_byte();
}
  
FRESULT TRS_FS_SERIAL::f_open (
                               FIL* fp,           /* [OUT] Pointer to the file object structure */
                               const TCHAR* path, /* [IN] File name */
                               BYTE mode          /* [IN] Mode flags */
                               ) {
  send_marker();
  write_byte(F_OPEN);
  write_byte(mode);
  write_string(path);
  FRESULT fr = read_fresult();
  if (fr == FR_OK) {
    fp->f = (void*) read_word();
  }
  return fr;
}

FRESULT TRS_FS_SERIAL::f_opendir (
                                  DIR_* dp,           /* [OUT] Pointer to the directory object structure */
                                  const TCHAR* path  /* [IN] Directory name */
                                  ) {
  send_marker();
  write_byte(F_OPENDIR);
  write_string(path);
  FRESULT fr = read_fresult();
  if (fr == FR_OK) {
    dp->dir = (void*) read_word();
  }
  return fr;
}

FRESULT TRS_FS_SERIAL::f_write (
                                FIL* fp,          /* [IN] Pointer to the file object structure */
                                const void* buff, /* [IN] Pointer to the data to be written */
                                UINT btw,         /* [IN] Number of bytes to write */
                                UINT* bw          /* [OUT] Pointer to the variable to return number of bytes written */
                                ) {
  send_marker();
  write_byte(F_WRITE);
  write_word((uint32_t) fp->f);
  write_word(btw);
  write_blob(buff, btw);
#if 0
  uint8_t* p = (uint8_t*) buff;
  for (int i = 0; i < btw; i++) {
    write_byte(*p++);
  }
#endif
  FRESULT fr = read_fresult();
  *bw = read_word();
  return fr;
}

FRESULT TRS_FS_SERIAL::f_read (
                               FIL* fp,     /* [IN] File object */
                               void* buff,  /* [OUT] Buffer to store read data */
                               UINT btr,    /* [IN] Number of bytes to read */
                               UINT* br     /* [OUT] Number of bytes read */
                               ) {
  send_marker();
  write_byte(F_READ);
  write_word((uint32_t) fp->f);
  write_word(btr);
  FRESULT fr = read_fresult();
  *br = read_word();
  read_blob(buff, *br);
#if 0
  uint8_t* p = (uint8_t*) buff;
  for (int i = 0; i < *br; i++) {
    *p++ = read_byte();
  }
#endif
  return fr;
}

FRESULT TRS_FS_SERIAL::f_readdir (
                                  DIR_* dp,      /* [IN] Directory object */
                                  FILINFO* fno  /* [OUT] File information structure */
                                  ) {
  send_marker();
  write_byte(F_READDIR);
  write_word((uint32_t) dp->dir);
  FRESULT fr = read_fresult();
  if (fr == FR_OK) {
    read_string(fno->fname);
    fno->fsize = read_long();
    fno->fattrib = read_byte();
  }
  return fr;
}

FSIZE_t TRS_FS_SERIAL::f_tell (
                               FIL* fp   /* [IN] File object */
                               ) {
  assert(0);
}

FRESULT TRS_FS_SERIAL::f_sync (
                               FIL* fp     /* [IN] File object */
                               ) {
  assert(0);
}

FRESULT TRS_FS_SERIAL::f_lseek (
                                FIL*    fp,  /* [IN] File object */
                                FSIZE_t ofs  /* [IN] File read/write pointer */
                                ) {
  send_marker();
  write_byte(F_LSEEK);
  write_word((uint32_t) fp->f);
  write_long(ofs);
  FRESULT fr = read_fresult();
  return fr;
}
  
FRESULT TRS_FS_SERIAL::f_close (
                                FIL* fp     /* [IN] Pointer to the file object */
                                ) {
  send_marker();
  write_byte(F_CLOSE);
  write_word((uint32_t) fp->f);
  FRESULT fr = read_fresult();
  return fr;
}

FRESULT TRS_FS_SERIAL::f_unlink (
                                 const TCHAR* path  /* [IN] Object name */
                                 ) {
  send_marker();
  write_byte(F_UNLINK);
  write_string(path);
  FRESULT fr = read_fresult();
  return fr;
}

FRESULT TRS_FS_SERIAL::f_stat (
                               const TCHAR* path,  /* [IN] Object name */
                               FILINFO* fno        /* [OUT] FILINFO structure */
                               ) {
  send_marker();
  write_byte(F_STAT);
  write_string(path);
  FRESULT fr = read_fresult();
  if (fr == FR_OK) {
    strcpy(fno->fname, path);
    fno->fsize = read_long();
    fno->fattrib = read_byte();
  }
  return fr;
}
