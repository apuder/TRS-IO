
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "fileio.h"
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

enum FS_TYPE {
  FS_SMB,
  FS_POSIX
};

const char* init_trs_fs_posix();
const char* init_trs_fs_smb();
const char* init_trs_fs_smb(const char* url, const char* user, const char* passwd);
const char* get_smb_err_msg();
const char* get_posix_err_msg();
const char* get_frehd_msg();
bool trs_fs_has_sd_card_reader();


class TRS_FS {
private:
  SemaphoreHandle_t xSemaphore = NULL;

protected:
  const char* err_msg;

public:
  TRS_FS() {
    xSemaphore = xSemaphoreCreateMutex();
  }

  virtual ~TRS_FS() {}
  
  virtual FS_TYPE type() = 0;

  void lock() {
    xSemaphoreTake(xSemaphore, portMAX_DELAY);
  }

  void unlock() {
    xSemaphoreGive(xSemaphore);
  }

  const char* get_err_msg() {
    return err_msg;
  }

  virtual void f_log(const char* msg) = 0;

  virtual FRESULT f_open(FIL* fp,           /* [OUT] Pointer to the file object structure */
                         const TCHAR* path, /* [IN] File name */
                         BYTE mode) = 0;    /* [IN] Mode flags */

  virtual FRESULT f_opendir(DIR_* dp,               /* [OUT] Pointer to the directory object structure */
                            const TCHAR* path) = 0; /* [IN] Directory name */
  
  virtual FRESULT f_write(FIL* fp,          /* [IN] Pointer to the file object structure */
                          const void* buff, /* [IN] Pointer to the data to be written */
                          UINT btw,         /* [IN] Number of bytes to write */
                          UINT* bw) = 0;    /* [OUT] Pointer to the variable to return number of bytes written */

  virtual FRESULT f_read(FIL* fp,       /* [IN] File object */
                         void* buff,    /* [OUT] Buffer to store read data */
                         UINT btr,      /* [IN] Number of bytes to read */
                         UINT* br) = 0; /* [OUT] Number of bytes read */

  virtual FRESULT f_readdir(DIR_* dp,          /* [IN] Directory object */
                            FILINFO* fno) = 0; /* [OUT] File information structure */

  virtual FSIZE_t f_tell(FIL* fp) = 0;   /* [IN] File object */

  virtual FRESULT f_sync(FIL* fp) = 0;   /* [IN] File object */

  virtual FRESULT f_lseek(FIL*    fp,        /* [IN] File object */
                          FSIZE_t ofs) = 0;  /* [IN] File read/write pointer */

  virtual FRESULT f_close(FIL* fp) = 0;      /* [IN] Pointer to the file object */

  virtual FRESULT f_unlink(const TCHAR* path) = 0; /* [IN] Object name */

  virtual FRESULT f_stat(const TCHAR* path,  /* [IN] Object name */
                         FILINFO* fno) = 0;  /* [OUT] FILINFO structure */
};

extern TRS_FS* trs_fs;
