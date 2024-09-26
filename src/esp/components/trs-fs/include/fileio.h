
#ifndef __FILEIO_H__
#define __FILEIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <dirent.h>


#include <inttypes.h>

typedef uint8_t Uchar;
typedef uint8_t UCHAR;
typedef char CHAR;
typedef uint8_t BYTE;
typedef uint16_t UINT;
typedef uint16_t USHORT;
typedef uint32_t DWORD;
typedef int16_t WORD;

#define f_get_error _f_get_error
#define f_open _f_open
#define f_opendir _f_opendir
#define f_write _f_write
#define f_read _f_read
#define f_readdir _f_readdir
#define f_tell _f_tell
#define f_sync _f_sync
#define f_lseek _f_lseek
#define f_close _f_close
#define f_unlink _f_unlink
#define f_stat _f_stat


#ifndef FRESULT_DEFINED
#define FRESULT_DEFINED

typedef enum {
    FR_OK = 0,				/* (0) Succeeded */
    FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
    FR_INT_ERR,				/* (2) Assertion failed */
    FR_NOT_READY,			/* (3) The physical drive cannot work */
    FR_NO_FILE,				/* (4) Could not find the file */
    FR_NO_PATH,				/* (5) Could not find the path */
    FR_INVALID_NAME,		/* (6) The path name format is invalid */
    FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
    FR_EXIST,				/* (8) Access denied due to prohibited access */
    FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
    FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
    FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
    FR_NOT_ENABLED,			/* (12) The volume has no work area */
    FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
    FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
    FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
    FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
    FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
    FR_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > FF_FS_LOCK */
    FR_INVALID_PARAMETER,     /* (19) Given parameter is invalid */
    FR_DISK_FULL
} FRESULT;

#define	FA_READ				0x01
#define	FA_WRITE			0x02
#define	FA_OPEN_EXISTING	0x00
#define	FA_CREATE_NEW		0x04
#define	FA_CREATE_ALWAYS	0x08
#define	FA_OPEN_ALWAYS		0x10
#define	FA_OPEN_APPEND		0x30

#endif

#define FF_MAX_SS 256

typedef char TCHAR;
typedef uint32_t FSIZE_t;

typedef struct {
    void*   f;          /* Object identifier */
} FIL;

typedef struct {
    FSIZE_t fsize;               /* File size */
    WORD    fdate;               /* Last modified date */
    WORD    ftime;               /* Last modified time */
    BYTE    fattrib;             /* Attribute */
    TCHAR   fname[12 + 1];
} FILINFO;

typedef struct {
    void*  dir;        /* Object identifier */
} DIR_;

void f_log(const char* format, ...);

int trs_fs_mounted();

const char* f_get_error(FRESULT error);

FRESULT f_open (
        FIL* fp,           /* [OUT] Pointer to the file object structure */
        const TCHAR* path, /* [IN] File name */
        BYTE mode          /* [IN] Mode flags */
);

FRESULT f_opendir (
        DIR_* dp,           /* [OUT] Pointer to the directory object structure */
        const TCHAR* path  /* [IN] Directory name */
);

FRESULT f_write (
        FIL* fp,          /* [IN] Pointer to the file object structure */
        const void* buff, /* [IN] Pointer to the data to be written */
        UINT btw,         /* [IN] Number of bytes to write */
        UINT* bw          /* [OUT] Pointer to the variable to return number of bytes written */
);

FRESULT f_read (
        FIL* fp,     /* [IN] File object */
        void* buff,  /* [OUT] Buffer to store read data */
        UINT btr,    /* [IN] Number of bytes to read */
        UINT* br     /* [OUT] Number of bytes read */
);

FRESULT f_readdir (
        DIR_* dp,      /* [IN] Directory object */
        FILINFO* fno  /* [OUT] File information structure */
);

FSIZE_t f_tell (
        FIL* fp   /* [IN] File object */
);

FRESULT f_sync (
        FIL* fp     /* [IN] File object */
);

FRESULT f_lseek (
        FIL*    fp,  /* [IN] File object */
        FSIZE_t ofs  /* [IN] File read/write pointer */
);

FRESULT f_close (
        FIL* fp     /* [IN] Pointer to the file object */
);

FRESULT f_unlink (
        const TCHAR* path  /* [IN] Object name */
);

FRESULT f_stat (
        const TCHAR* path,  /* [IN] Object name */
        FILINFO* fno        /* [OUT] FILINFO structure */
);

#ifdef __cplusplus
}
#endif

#endif
