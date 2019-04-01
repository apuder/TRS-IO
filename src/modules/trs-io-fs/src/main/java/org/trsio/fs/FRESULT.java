package org.trsio.fs;

public class FRESULT {
    final public static byte FR_OK = 0;				/* (0) Succeeded */
    final public static byte FR_DISK_ERR = 1;		/* (1) A hard error occurred in the low level disk I/O layer */
    final public static byte FR_INT_ERR = 2;		/* (2) Assertion failed */
    final public static byte FR_NOT_READY = 3;		/* (3) The physical drive cannot work */
    final public static byte FR_NO_FILE = 4;		/* (4) Could not find the file */
    final public static byte FR_NO_PATH = 5;		/* (5) Could not find the path */
    final public static byte FR_INVALID_NAME = 6;	/* (6) The path name format is invalid */
    final public static byte FR_DENIED = 7;			/* (7) Access denied due to prohibited access or directory full */
    final public static byte FR_EXIST = 8;			/* (8) Access denied due to prohibited access */
    final public static byte FR_INVALID_OBJECT = 9;	/* (9) The file/directory object is invalid */
    final public static byte FR_WRITE_PROTECTED = 10;/* (10) The physical drive is write protected */
    final public static byte FR_INVALID_DRIVE = 11;	/* (11) The logical drive number is invalid */
    final public static byte FR_NOT_ENABLED = 12;	/* (12) The volume has no work area */
    final public static byte FR_NO_FILESYSTEM = 13;	/* (13) There is no valid FAT volume */
    final public static byte FR_MKFS_ABORTED = 14;	/* (14) The f_mkfs() aborted due to any problem */
    final public static byte FR_TIMEOUT = 15;		/* (15) Could not get a grant to access the volume within defined period */
    final public static byte FR_LOCKED = 16;		/* (16) The operation is rejected according to the file sharing policy */
    final public static byte FR_NOT_ENOUGH_CORE = 17;/* (17) LFN working buffer could not be allocated */
    final public static byte FR_TOO_MANY_OPEN_FILES = 18;/* (18) Number of open files > FF_FS_LOCK */
    final public static byte FR_INVALID_PARAMETER = 19; /* (19) Given parameter is invalid */
    final public static byte FR_DISK_FULL = 20;
}
