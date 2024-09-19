
#pragma once

#include <stdint.h>

typedef struct __PACKED__ _dir_t {
  const char fname[15];
  uint8_t    prot_level;
  uint8_t    eof;
  uint8_t    lrl;
  uint16_t   ern;
  uint16_t   ngran;
} dir_t;

#define MAX_DIR_ENTRIES 81

typedef uint8_t dos_err_t;

typedef dir_t dir_buf_t[MAX_DIR_ENTRIES];

typedef uint8_t sector_t[256];

#define DIR_ENTRY_SIZE(d) (((d)->lrl == 0 ? 256 : (d)->lrl) * ((d)->ern - ((d)->eof != 0)) + (d)->eof)

#define NO_ERR 0
#define ERR_ILLEGAL_FILENAME 19
#define ERR_EOF 28
#define ERR_REC_OUT_OF_RANGE 29
#define ERR_LRL 42
