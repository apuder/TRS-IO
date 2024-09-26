
#include "trs-fs.h"

#define CHECK() if(trs_fs == NULL || trs_fs->get_err_msg() != NULL) return FR_NOT_READY;

static const char *errors[] =
{
   "Unknown error",           /* 0  */
   "Disk error",              /* 1  */
   "Internal error",          /* 2  */
   "Drive not ready",         /* 3  */
   "File not found",          /* 4  */
   "Path not found",          /* 5  */
   "Invalid pathname",        /* 6  */
   "Access denied",           /* 7  */
   "File exists",             /* 8  */
   "File/dir object invalid", /* 9  */
   "Write protected",         /* 10 */
   "Invalid drive",           /* 11 */
   "Volume not mounted",      /* 12 */
   "No FAT fs found",         /* 13 */
   "mkfs aborted",            /* 14 */
   "Timeout detected",        /* 15 */
   "File locked",             /* 16 */
   "Not enough core",         /* 17 */
   "Too many open files",     /* 18 */
   "Invalid parameter",       /* 19 */
   "Disk full",               /* 20 */
};

const char* f_get_error(FRESULT error)
{
  if(!(error > 0 && error < sizeof(errors) / sizeof(errors[0]))) {
    error = FR_OK; /* unknown error */
  }
  return errors[error];
}

void f_log(const char* format, ...) {
#if 0
  char* msg;
  va_list args;
  va_start(args, format);
  vasprintf(&msg, format, args);
  va_end(args);
  trs_fs->f_log(msg);
  free(msg);
#endif
}

int trs_fs_mounted() {
  return trs_fs != NULL;
}

FRESULT f_open (
                FIL* fp,           /* [OUT] Pointer to the file object structure */
                const TCHAR* path, /* [IN] File name */
                BYTE mode          /* [IN] Mode flags */
                ) {
  f_log("f_open: %s", path);
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_open(fp, path, mode);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_opendir (
                   DIR_* dp,           /* [OUT] Pointer to the directory object structure */
                   const TCHAR* path  /* [IN] Directory name */
                   ) {
  f_log("f_opendir: %s", path);
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_opendir(dp, path);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_write (
                 FIL* fp,          /* [IN] Pointer to the file object structure */
                 const void* buff, /* [IN] Pointer to the data to be written */
                 UINT btw,         /* [IN] Number of bytes to write */
                 UINT* bw          /* [OUT] Pointer to the variable to FRESULT err = number of bytes written */
                 ) {
  f_log("f_write");
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_write(fp, buff, btw, bw);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_read (
                FIL* fp,     /* [IN] File object */
                void* buff,  /* [OUT] Buffer to store read data */
                UINT btr,    /* [IN] Number of bytes to read */
                UINT* br     /* [OUT] Number of bytes read */
                ) {
  f_log("f_read");
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_read(fp, buff, btr, br);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_readdir (
                   DIR_* dp,      /* [IN] Directory object */
                   FILINFO* fno  /* [OUT] File information structure */
                   ) {
  f_log("f_readdir");
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_readdir(dp, fno);
  trs_fs->unlock();
  f_log("fn : %s", fno->fname);
  f_log("err: %d", err);
  return err;
}

FSIZE_t f_tell (
                FIL* fp   /* [IN] File object */
                ) {
  f_log("f_tell");
  CHECK();
  trs_fs->lock();
  FSIZE_t size = trs_fs->f_tell(fp);
  trs_fs->unlock();
  f_log("size: %d", size);
  return size;
}

FRESULT f_sync (
                FIL* fp     /* [IN] File object */
                ) {
  f_log("f_sync");
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_sync(fp);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_lseek (
                 FIL*    fp,  /* [IN] File object */
                 FSIZE_t ofs  /* [IN] File read/write pointer */
                 ) {
  f_log("f_lseek");
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_lseek(fp, ofs);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_close (
                 FIL* fp     /* [IN] Pointer to the file object */
                 ) {
  f_log("f_close");
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_close(fp);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_unlink (
                  const TCHAR* path  /* [IN] Object name */
                  ) {
  f_log("f_unlink: %s", path);
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_unlink(path);
  trs_fs->unlock();
  f_log("err: %d", err);
  return err;
}

FRESULT f_stat (
                const TCHAR* path,  /* [IN] Object name */
                FILINFO* fno        /* [OUT] FILINFO structure */
                ) {
  f_log("f_stat: %s", path);
  CHECK();
  trs_fs->lock();
  FRESULT err = trs_fs->f_stat(path, fno);
  f_log("err: %d", err);
  trs_fs->unlock();
  return err;
}
