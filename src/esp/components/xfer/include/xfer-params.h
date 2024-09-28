#ifndef __XDEF_PARAMS_H__
#define __XDEF_PARAMS_H__

#ifdef ESP_PLATFORM
#define __PACKED__ __attribute__ ((packed))
#else
#define __PACKED__
#endif

#ifdef ESP_PLATFORM
#include "dos.h"
#endif

#define XFER_MODULE_ID 5
#define XFER_START 0
#define XFER_STOP  1
#define XFER_NEXT_CMD 2
#define XFER_SEND_RESULT 3

#define XFER_CMD_DIR 0
#define XFER_CMD_OPEN 1
#define XFER_CMD_INIT 2
#define XFER_CMD_READ 3
#define XFER_CMD_WRITE 4
#define XFER_CMD_CLOSE 5
#define XFER_CMD_REMOVE 6
#define XFER_CMD_RENAME 7

#define MAX_FNAME_LEN 15


typedef struct __PACKED__ _cmd_in_dir_t {
  uint8_t drive;
} cmd_in_dir_t;

typedef struct __PACKED__ _cmd_out_dir_t {
  dos_err_t err;
  uint16_t n;
  dir_buf_t entries;
} cmd_out_dir_t;

typedef struct __PACKED__ _cmd_in_open_t {
  char fn[MAX_FNAME_LEN + 1];
} cmd_in_open_t;

typedef struct __PACKED__ _cmd_out_open_t {
  dos_err_t err;
} cmd_out_open_t;

typedef struct __PACKED__ _cmd_in_init_t {
  char fn[MAX_FNAME_LEN + 1];
} cmd_in_init_t;

typedef struct __PACKED__ _cmd_out_init_t {
  dos_err_t err;
} cmd_out_init_t;

typedef struct __PACKED__ _cmd_in_write_t {
  uint16_t count;
  sector_t sector;
} cmd_in_write_t;

typedef struct __PACKED__ _cmd_out_write_t {
  dos_err_t err;
} cmd_out_write_t;

typedef void* cmd_in_read_t;

typedef struct __PACKED__ _cmd_out_read_t {
  dos_err_t err;
  uint16_t count;
  sector_t sector;
} cmd_out_read_t;

typedef void* cmd_in_close_t;

typedef struct __PACKED__ _cmd_out_close_t {
  dos_err_t err;
} cmd_out_close_t;

typedef struct __PACKED__ _cmd_in_remove_t {
  char fn[MAX_FNAME_LEN + 1];
} cmd_in_remove_t;

typedef struct __PACKED__ _cmd_out_remove_t {
  dos_err_t err;
} cmd_out_remove_t;

typedef struct __PACKED__ _cmd_in_rename_t {
  char from[MAX_FNAME_LEN + 1];
  char to[MAX_FNAME_LEN + 1];
} cmd_in_rename_t;

typedef struct __PACKED__ _cmd_out_rename_t {
  dos_err_t err;
} cmd_out_rename_t;

typedef union __PACKED__ _params_t {
  cmd_in_dir_t dir;
  cmd_in_open_t open;
  cmd_in_init_t init;
  cmd_in_write_t write;
  cmd_in_read_t read;
  cmd_in_close_t close;
  cmd_in_remove_t remove;
  cmd_in_rename_t rename;
} params_t;

typedef struct __PACKED__ _xfer_in_t {
#ifdef __cplusplus
  _xfer_in_t() {};
#endif
  uint8_t cmd;
  params_t params;
} xfer_in_t;

typedef union __PACKED__ _xfer_out_t {
#ifdef __cplusplus
  _xfer_out_t() {};
#endif
  cmd_out_dir_t dir;
  cmd_out_open_t open;
  cmd_out_init_t init;
  cmd_out_write_t write;
  cmd_out_read_t read;
  cmd_out_close_t close;
  cmd_out_remove_t remove;
  cmd_out_rename_t rename;
} xfer_out_t;

#endif
