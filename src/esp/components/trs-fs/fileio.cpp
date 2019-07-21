
#include "trs-fs.h"


void f_log(const char* format, ...) {
  trs_fs->f_log(format);
}

FRESULT f_open (
                FIL* fp,           /* [OUT] Pointer to the file object structure */
                const TCHAR* path, /* [IN] File name */
                BYTE mode          /* [IN] Mode flags */
                ) {
  return trs_fs->f_open(fp, path, mode);
}

FRESULT f_opendir (
                   DIR_* dp,           /* [OUT] Pointer to the directory object structure */
                   const TCHAR* path  /* [IN] Directory name */
                   ) {
  return trs_fs->f_opendir(dp, path);
}

FRESULT f_write (
                 FIL* fp,          /* [IN] Pointer to the file object structure */
                 const void* buff, /* [IN] Pointer to the data to be written */
                 UINT btw,         /* [IN] Number of bytes to write */
                 UINT* bw          /* [OUT] Pointer to the variable to return number of bytes written */
                 ) {
  return trs_fs->f_write(fp, buff, btw, bw);
}

FRESULT f_read (
                FIL* fp,     /* [IN] File object */
                void* buff,  /* [OUT] Buffer to store read data */
                UINT btr,    /* [IN] Number of bytes to read */
                UINT* br     /* [OUT] Number of bytes read */
                ) {
  return trs_fs->f_read(fp, buff, btr, br);
}

FRESULT f_readdir (
                   DIR_* dp,      /* [IN] Directory object */
                   FILINFO* fno  /* [OUT] File information structure */
                   ) {
  return trs_fs->f_readdir(dp, fno);
}

FSIZE_t f_tell (
                FIL* fp   /* [IN] File object */
                ) {
  return trs_fs->f_tell(fp);
}

FRESULT f_sync (
                FIL* fp     /* [IN] File object */
                ) {
  return trs_fs->f_sync(fp);
}

FRESULT f_lseek (
                 FIL*    fp,  /* [IN] File object */
                 FSIZE_t ofs  /* [IN] File read/write pointer */
                 ) {
  return trs_fs->f_lseek(fp, ofs);
}

FRESULT f_close (
                 FIL* fp     /* [IN] Pointer to the file object */
                 ) {
  return trs_fs->f_close(fp);
}

FRESULT f_unlink (
                  const TCHAR* path  /* [IN] Object name */
                  ) {
  return trs_fs->f_unlink(path);
}

FRESULT f_stat (
                const TCHAR* path,  /* [IN] Object name */
                FILINFO* fno        /* [OUT] FILINFO structure */
                ) {
  return trs_fs->f_stat(path, fno);
}
