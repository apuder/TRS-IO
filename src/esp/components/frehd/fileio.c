
#include "fileio.h"
#ifdef ESP_PLATFORM
#include "serial-io.h"
#else
#include "socket-io.h"
#endif
#include <string.h>
#include <assert.h>
#include <stdarg.h>

#define F_LOG 99
#define F_OPEN 0
#define F_READ 1
#define F_WRITE 2
#define F_CLOSE 3
#define F_UNLINK 4
#define F_LSEEK 5
#define F_OPENDIR 6
#define F_READDIR 7
#define F_STAT 8


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

void f_log(const char* format, ...) {
  char buf[80];
  va_list args;
  va_start(args, format);
  vsprintf(buf,format, args);
  va_end(args);
  send_marker();
  write_byte(F_LOG);
  write_string(buf);
  read_byte();
}

FRESULT f_open (
        FIL* fp,           /* [OUT] Pointer to the file object structure */
        const TCHAR* path, /* [IN] File name */
        BYTE mode          /* [IN] Mode flags */
) {
    send_marker();
    write_byte(F_OPEN);
    write_byte(mode);
    write_string(path);
    FRESULT fr = read_byte();
    if (fr == FR_OK) {
        fp->f = (FILE*) read_word();
    }
    return fr;
}

FRESULT f_opendir (
        DIR_* dp,           /* [OUT] Pointer to the directory object structure */
        const TCHAR* path  /* [IN] Directory name */
) {
    send_marker();
    write_byte(F_OPENDIR);
    write_string(path);
    FRESULT fr = read_byte();
    if (fr == FR_OK) {
        dp->dir = (DIR*) read_word();
    }
    return fr;
}

FRESULT f_write (
        FIL* fp,          /* [IN] Pointer to the file object structure */
        const void* buff, /* [IN] Pointer to the data to be written */
        UINT btw,         /* [IN] Number of bytes to write */
        UINT* bw          /* [OUT] Pointer to the variable to return number of bytes written */
) {
    send_marker();
    write_byte(F_WRITE);
    write_word((uint16_t) fp->f);
    write_word(btw);
    write_blob(buff, btw);
#if 0
    uint8_t* p = (uint8_t*) buff;
    for (int i = 0; i < btw; i++) {
        write_byte(*p++);
    }
#endif
    FRESULT fr = read_byte();
    *bw = read_word();
    return fr;
}

FRESULT f_read (
        FIL* fp,     /* [IN] File object */
        void* buff,  /* [OUT] Buffer to store read data */
        UINT btr,    /* [IN] Number of bytes to read */
        UINT* br     /* [OUT] Number of bytes read */
) {
    send_marker();
    write_byte(F_READ);
    write_word((uint16_t) fp->f);
    write_word(btr);
    FRESULT fr = read_byte();
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

FRESULT f_readdir (
        DIR_* dp,      /* [IN] Directory object */
        FILINFO* fno  /* [OUT] File information structure */
) {
    send_marker();
    write_byte(F_READDIR);
    write_word((uint16_t) dp->dir);
    FRESULT fr = read_byte();
    if (fr == FR_OK) {
        read_string(fno->fname);
        fno->fsize = read_long();
        fno->fattrib = read_byte();
    }
    return fr;
}

FSIZE_t f_tell (
        FIL* fp   /* [IN] File object */
) {
    assert(0);
}

FRESULT f_sync (
        FIL* fp     /* [IN] File object */
) {
    assert(0);
}

FRESULT f_lseek (
        FIL*    fp,  /* [IN] File object */
        FSIZE_t ofs  /* [IN] File read/write pointer */
) {
    send_marker();
    write_byte(F_LSEEK);
    write_word((uint16_t) fp->f);
    write_long(ofs);
    FRESULT fr = read_byte();
    return fr;
}

FRESULT f_close (
        FIL* fp     /* [IN] Pointer to the file object */
) {
    send_marker();
    write_byte(F_CLOSE);
    write_word((uint16_t) fp->f);
    FRESULT fr = read_byte();
    return fr;
}

FRESULT f_unlink (
        const TCHAR* path  /* [IN] Object name */
) {
    send_marker();
    write_byte(F_UNLINK);
    write_string(path);
    FRESULT fr = read_byte();
    return fr;
}

FRESULT f_stat (
        const TCHAR* path,  /* [IN] Object name */
        FILINFO* fno        /* [OUT] FILINFO structure */
) {
    send_marker();
    write_byte(F_STAT);
    write_string(path);
    FRESULT fr = read_byte();
    if (fr == FR_OK) {
        strcpy(fno->fname, path);
        fno->fsize = read_long();
        fno->fattrib = read_byte();
    }
    return fr;
}

