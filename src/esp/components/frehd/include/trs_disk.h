/*
 * Copyright (c) 1996-2020, Timothy P. Mann
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Emulate Model-I or Model-III disk controller
 */

#include "integer.h"

extern void trs_disk_init(int reset_button);
extern void trs_disk_select_write(Uint8 data);
extern Uint8 trs_disk_track_read(void);
extern void trs_disk_track_write(Uint8 data);
extern Uint8 trs_disk_sector_read(void);
extern void trs_disk_sector_write(Uint8 data);
extern Uint8 trs_disk_data_read(void);
extern void trs_disk_data_write(Uint8 data);
extern Uint8 trs_disk_status_read(void);
extern void trs_disk_command_write(Uint8 cmd);
extern Uint8 trs_disk_interrupt_read(void); /* M3 only */
extern void trs_disk_interrupt_write(Uint8 mask); /* M3 only */

#ifdef __linux
extern void trs_disk_setstep(int unit, int value);
extern int trs_disk_getstep(int unit);
#endif
extern void trs_disk_setsize(int unit, int value);
extern int trs_disk_getsize(int unit);
extern char* trs_disk_getfilename(int unit);
extern int trs_disk_getwriteprotect(int unit);
extern int trs_disk_getdisktype(int unit);

extern void trs_disk_insert(int drive, const char *diskname);
extern void trs_disk_remove(int drive);

extern int trs_diskset_save(const char *filename);
extern int trs_diskset_load(const char *filename);

extern int trs_disk_doubler;
extern char trs_disk_dir[FILENAME_MAX];
extern int trs_disk_truedam;

/* Values for emulated disk image type (emutype) */
#define JV1 1 /* compatible with Vavasour Model I emulator */
#define JV3 3 /* compatible with Vavasour Model III/4 emulator */
#define DMK 4 /* compatible with Keil Model III/4 emulator */
#define REAL 100 /* real floppy drive, PC controller */
#define NONE 0

/* Values for trs_disk_doubler flag word */
#define TRSDISK_NODOUBLER 0
#define TRSDISK_PERCOM    1
#define TRSDISK_TANDY     2
#define TRSDISK_BOTH      3

/* Model I drive select register -- address bits 0,1 not decoded */
#define TRSDISK_SELECT(addr) (((addr)&~3) == 0x37e0)
#define TRSDISK_0       0x1
#define TRSDISK_1       0x2
#define TRSDISK_2       0x4
#define TRSDISK_3       0x8
#define TRSDISK_SIDE    0x8     /* shared on Model I */
#define TRSDISK_4       0x3     /* fake value for emulator only */
#define TRSDISK_5       0x5     /* fake value for emulator only */
#define TRSDISK_6       0x6     /* fake value for emulator only */
#define TRSDISK_7       0x7     /* fake value for emulator only */

/* FDC address space in Model I */
#define TRSDISK_FDC     0x37ec
#define TRSDISK_FDCLEN  4       /* 4 bytes are mapped, offsets as follows: */
#define TRSDISK_COMMAND 0x37ec  /* writing */
#define TRSDISK_STATUS  0x37ec  /* reading */
#define TRSDISK_TRACK   0x37ed
#define TRSDISK_SECTOR  0x37ee
#define TRSDISK_DATA    0x37ef

/* FDC port space in Model III */
#define TRSDISK3_INTERRUPT 0xe4
#define TRSDISK3_COMMAND   0xf0  /* writing */
#define TRSDISK3_STATUS    0xf0  /* reading */
#define TRSDISK3_TRACK     0xf1
#define TRSDISK3_SECTOR    0xf2
#define TRSDISK3_DATA      0xf3
#define TRSDISK3_SELECT    0xf4  /* write-only */

/* Interrupt register bits (M3 only) */
#define TRSDISK3_INTRQ     0x80
#define TRSDISK3_DRQ       0x40
#define TRSDISK3_RESET     0x20  /* reset button, not really disk related */

/* Select register bits (M3 only). Drives 0-3 same as model 1. */
#define TRSDISK3_MFM       0x80
#define TRSDISK3_WAIT      0x40
#define TRSDISK3_PRECOMP   0x20
#define TRSDISK3_SIDE      0x10  /* not shared! */

/* Commands */
#define TRSDISK_CMDMASK  0xf0  /* high nybble selects which command */

/* Type I commands: cccchvrr, where
   cccc = command number
   h = head load
   v = verify (i.e., read next address to check we're on the right track)
   rr = step rate:  00=6ms, 01=12ms, 10=20ms, 11=40ms
*/
#define TRSDISK_RESTORE  0x00
#define TRSDISK_SEEK     0x10
#define TRSDISK_STEP     0x20  /* don't update track reg */
#define TRSDISK_STEPU    0x30  /* do update track reg */
#define TRSDISK_STEPIN   0x40
#define TRSDISK_STEPINU  0x50
#define TRSDISK_STEPOUT  0x60
#define TRSDISK_STEPOUTU 0x70
#define TRSDISK_UBIT     0x10
#define TRSDISK_HBIT     0x08
#define TRSDISK_VBIT     0x04

/* Type II commands: ccccbecd, where
   cccc = command number
   e = delay for head engage (10ms)
  1771:
   b = 1=IBM format, 0=nonIBM format
   cd = select data address mark (writes only, 00 for reads):
        00=FB (normal), 01=FA, 10=F9, 11=F8 (deleted)
  1791:
   b = side expected
   c = side compare (0=disable, 1=enable)
   d = select data address mark (writes only, 0 for reads):
       0=FB (normal), 1=F8 (deleted)
*/
#define TRSDISK_READ     0x80  /* single sector */
#define TRSDISK_READM    0x90  /* multiple sectors */
#define TRSDISK_WRITE    0xa0
#define TRSDISK_WRITEM   0xb0
#define TRSDISK_MBIT     0x10
#define TRSDISK_BBIT     0x08
#define TRSDISK_EBIT     0x04
#define TRSDISK_CBIT     0x02
#define TRSDISK_DBIT     0x01

/* Type III commands: ccccxxxs (?), where
   cccc = command number
   xxx = ?? (usually 010)
   s = 1=READTRK no synchronize; otherwise 0
*/
#define TRSDISK_READADR  0xc0
#define TRSDISK_READTRK  0xe0
#define TRSDISK_WRITETRK 0xf0
/* These commands are peculiar to the Percom Doubler */
#define TRSDISK_P1771    0xfe  /* Select 1771 single density controller */
#define TRSDISK_P1791    0xff  /* Select 1791 double density controller */

/* Type IV command: cccciiii, where
   cccc = command number
   iiii = bitmask of events to terminate and interrupt on (unused on trs80);
          0000 for immediate terminate with no interrupt.
 */
#define TRSDISK_FORCEINT 0xd0

/* These "commands" are peculiar to the Radio Shack Doubler.  They
   are written to the sector register, not the command register!
 */
#define TRSDISK_R1791    0x80
#define TRSDISK_R1771    0xa0
#define TRSDISK_NOPRECMP 0xc0
#define TRSDISK_PRECMP   0xe0

/* Type I status bits */
#define TRSDISK_BUSY     0x01
#define TRSDISK_INDEX    0x02
#define TRSDISK_TRKZERO  0x04
#define TRSDISK_CRCERR   0x08
#define TRSDISK_SEEKERR  0x10
#define TRSDISK_HEADENGD 0x20
#define TRSDISK_WRITEPRT 0x40
#define TRSDISK_NOTRDY   0x80

/* Read status bits */
/*      TRSDISK_BUSY     0x01*/
#define TRSDISK_DRQ      0x02
#define TRSDISK_LOSTDATA 0x04
/*      TRSDISK_CRCERR   0x08*/
#define TRSDISK_NOTFOUND 0x10
#define TRSDISK_RECTYPE  0x60
#define TRSDISK_1771_FB  0x00
#define TRSDISK_1771_FA  0x20
#define TRSDISK_1771_F9  0x40
#define TRSDISK_1771_F8  0x60
#define TRSDISK_1791_FB  0x00
#define TRSDISK_1791_F8  0x20
/*      TRSDISK_NOTRDY   0x80*/

/* Write status bits */
/*      TRSDISK_BUSY     0x01*/
/*      TRSDISK_DRQ      0x02*/
/*      TRSDISK_LOSTDATA 0x04*/
/*      TRSDISK_CRCERR   0x08*/
/*      TRSDISK_NOTFOUND 0x10*/
#define TRSDISK_WRITEFLT 0x20
/*      TRSDISK_WRITEPRT 0x40*/
/*      TRSDISK_NOTRDY   0x80*/

/* Read address status bits */
/*      TRSDISK_BUSY     0x01*/
/*      TRSDISK_DRQ      0x02*/
/*      TRSDISK_LOSTDATA 0x04*/
/*      TRSDISK_CRCERR   0x08*/
/*      TRSDISK_NOTFOUND 0x10*/
/*      unused, mbz      0x60*/
/*      TRSDISK_NOTRDY   0x80*/

/* Read track status bits */
/*      TRSDISK_BUSY     0x01*/
/*      TRSDISK_DRQ      0x02*/
/*      TRSDISK_LOSTDATA 0x04*/
/*      unused, mbz      0x78*/
/*      TRSDISK_NOTRDY   0x80*/

/* Write track status bits */
/*      TRSDISK_BUSY     0x01*/
/*      TRSDISK_DRQ      0x02*/
/*      TRSDISK_LOSTDATA 0x04*/
/*      unused, mbz      0x18*/
/*      TRSDISK_WRITEFLT 0x20*/
/*      TRSDISK_WRITEPRT 0x40*/
/*      TRSDISK_NOTRDY   0x80*/

