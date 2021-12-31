
#include "sdkconfig.h"

#ifdef CONFIG_TRS_IO_MODEL_1
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
 * Emulate Model I or III/4 disk controller
 */

/*
 * Debug flags.  Update help_message in debug.c if these change.
 */
#define DISKDEBUG_FDCREG   (1 << 0)  /* FDC register reads and writes */
#define DISKDEBUG_FDCCMD   (1 << 1)  /* FDC commands */
#define DISKDEBUG_VTOS3    (1 << 2)  /* VTOS 3.0 JV3 kludges */
#define DISKDEBUG_GAPS     (1 << 3)  /* Gaps and real_writetrk */
#define DISKDEBUG_REALSIZE (1 << 4)  /* REAL sector size detection */
#define DISKDEBUG_READADR  (1 << 5)  /* Read Address timing */
#define DISKDEBUG_DMK      (1 << 6)  /* DMK support */
#define DISKDEBUG_REALERR  (1 << 7)  /* ioctl errors accessing real disks */

#define TSTATEREV 1       /* Index holes timed by T-states, not real time */
#define SIZERETRY 1       /* Retry in different sizes on real_read */
#define DMK_MARK_IAM 0    /* Mark IAMs in track header; poor idea */
#define NDRIVES 2

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#if __linux
#include <fcntl.h>
#include <linux/fd.h>
#include <linux/fdreg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

//#include "trs.h"
#include "integer.h"
#include "crc.h"
//#include "error.h"
#include "trs_disk.h"
#include "trs_hard.h"
//#include "trs_stringy.h"
//#include "trs_state_save.h"
#include "esp_attr.h"

typedef unsigned long long tstate_t;

int trs_disk_nocontroller;
int trs_disk_doubler = TRSDISK_BOTH;
float trs_disk_holewidth = 0.01;
int trs_disk_truedam;
int trs_disk_debug_flags;

typedef struct {
  /* Registers */
  Uint8 status;
  Uint8 track;
  Uint8 sector;
  Uint8 data;
  /* Other state */
  Uint8 currcommand;
  int lastdirection;
  int bytecount;		/* bytes left to transfer this command */
  int format;			/* write track state machine */
  int format_bytecount;         /* bytes left in this part of write track */
  int format_sec;               /* current sector number or id_index */
  int format_gapcnt;            /* measure requested gaps */
  int format_gap[5];
  Uint16 crc;
  unsigned curdrive;
  int curside;
  int density;			/* sden=0, dden=1 */
  Uint8 controller;		/* TRSDISK_P1771 or TRSDISK_P1791 */
  int last_readadr;             /* id index found by last readadr */
  tstate_t motor_timeout;       /* 0 if stopped, else time when it stops */
} FDCState;

static FDCState state, other_state;

/* Format states - what is expected next? */
#define FMT_GAP0    0
#define FMT_IAM     1
#define FMT_GAP1    2
#define FMT_IDAM    3
#define FMT_TRACKID 4
#define FMT_HEADID  5
#define FMT_SECID   6
#define FMT_SIZEID  7
#define FMT_IDCRC   8
#define FMT_GAP2    9
#define FMT_DAM     10
#define FMT_DATA    11
#define FMT_DCRC    12
#define FMT_GAP3    13
#define FMT_GAP4    14
#define FMT_DONE    15
#define FMT_PREAM   16  /* DDEN DMK only -- just saw preamble to an AM */
#define FMT_IPREAM  17  /* DDEN DMK only -- just saw preamble to an IAM */


/* Gap 0+1 and gap 4 angular size, used in Read Address timing emulation.
   Units: fraction of a complete circle. */
#define GAP1ANGLE 0.020
#define GAP4ANGLE 0.050

/* How long does emulated motor stay on after drive selected? (us of
   emulated time) */
#define MOTOR_USEC 2000000

/* Heuristic: how often are we willing to check whether real drive
   has a disk in it?  (seconds of real time) */
#define EMPTY_TIMEOUT 3

/*
 * The following rather quirky data structure is designed to be
 * compatible with what Jeff Vavasour's Model III/4 emulator uses to
 * represent disk formatting information.  My interpretation is based
 * on reading his documentation, looking at some disk images, and
 * experimenting with his emulator to generate odd cases, so the
 * compatibility should be excellent.
 *
 * I have compatibly extended the format to allow for more sectors, so
 * that 8" DSDD drives can be supported, by adding a second block of
 * ids after the block of data sectors that is described by the first
 * block.  JV himself says that sounds like a good idea.  Matthew
 * Reed's emulators now support this extension.
 *
 * I've further extended the format to add a flag bit for non-IBM
 * sectors. Only a subset of the non-IBM functionality is supported,
 * rigged to make the VTOS 3.0 copy-protection system work.  Non-IBM
 * sectors were a feature of the 1771 only. Using this feature, you
 * could have a sector of any length from 16 to 4096 bytes in
 * multiples of 16. xtrs supports only 16-byte non-IBM sectors (with
 * their data stored in a 256-byte field), and has some special kludges
 * to detect and support VTOS's trick of formatting these sectors with
 * inadequate gaps between so that writing to one would smash the
 * index block of the next one.
 *
 * Finally, I've extended the format to support (IBM) sector lengths
 * other than 256 bytes. The standard lengths 128, 256, 512, and 1024
 * are supported, using the last two available bits in the header
 * flags byte. The data area of a floppy image thus becomes an array
 * of *variable length* sectors, making it more complicated to find
 * the sector data corresponding to a given header and to manage freed
 * sectors.
 *
 * NB: JV's model I emulator uses no auxiliary data structure for disk
 * format.  It simply assumes that all disks are single density, 256
 * bytes/sector, 10 sectors/track, single sided, with nonstandard FA
 * data address mark on all sectors on track 17.  */

/* Values for flags below */
#define JV3_DENSITY     0x80  /* 1=dden, 0=sden */
#define JV3_DAM         0x60  /* data address mark; values follow */
#define JV3_DAMSDFB     0x00
#define JV3_DAMSDFA     0x20
#define JV3_DAMSDF9     0x40
#define JV3_DAMSDF8     0x60
#define JV3_DAMDDFB     0x00
#define JV3_DAMDDF8     0x20
#define JV3_SIDE        0x10  /* 0=side 0, 1=side 1 */
#define JV3_ERROR       0x08  /* 0=ok, 1=CRC error */
#define JV3_NONIBM      0x04  /* 0=normal, 1=short (for VTOS 3.0, xtrs only) */
#define JV3_SIZE        0x03  /* in used sectors: 0=256,1=128,2=1024,3=512
				 in free sectors: 0=512,1=1024,2=128,3=256 */

#define JV3_FREE        0xff  /* in track/sector fields */
#define JV3_FREEF       0xfc  /* in flags field, or'd with size code */

typedef struct {
  Uint8 track;
  Uint8 sector;
  Uint8 flags;
} SectorId;

#if 0
#define MAXTRACKS   255
#define JV1_SECSIZE 256
#define MAXSECSIZE 1024
#else
#define MAXTRACKS   80
#define JV1_SECSIZE 256
#define MAXSECSIZE 256
#endif

/* Max bytes per unformatted track. */
/* Select codes 1, 2, 4, 8 are emulated 5" drives, disk?-0 to disk?-3 */
#define TRKSIZE_SD    3125  /* 250kHz / 5 Hz [300rpm] / (2 * 8) */
                         /* or 300kHz / 6 Hz [360rpm] / (2 * 8) */
#define TRKSIZE_DD    6250  /* 250kHz / 5 Hz [300rpm] / 8 */
                         /* or 300kHz / 6 Hz [360rpm] / 8 */
/* Select codes 3, 5, 6, 7 are emulated 8" drives, disk?-4 to disk?-7 */
#define TRKSIZE_8SD   5208  /* 500kHz / 6 Hz [360rpm] / (2 * 8) */
#define TRKSIZE_8DD  10416  /* 500kHz / 6 Hz [360rpm] / 8 */
/* TRS-80 software has no concept of HD, so these constants are unused,
 * but expanded JV3 would be big enough even for 3.5" HD. */
#define TRKSIZE_5HD  10416  /* 500kHz / 6 Hz [360rpm] / 8 */
#define TRKSIZE_3HD  12500  /* 500kHz / 5 Hz [300rpm] / 8 */

#define JV3_SIDES      2
#define JV3_IDSTART    0
#define JV3_SECSTART   (34*256) /* start of sectors within file */
#define JV3_SECSPERBLK ((int)(JV3_SECSTART/3))
#define JV3_SECSMAX    (2*JV3_SECSPERBLK)

#define JV1_SECPERTRK 10

typedef struct {
  int free_id[4];		  /* first free id, if any, of each size */
  int last_used_id;		  /* last used index */
  int nblocks;                    /* number of blocks of ids, 1 or 2 */
  int sorted_valid;               /* sorted_id array valid */
#if 0
  SectorId id[JV3_SECSMAX + 1];   /* extra one is a loop sentinel */
  int offset[JV3_SECSMAX + 1];    /* offset into file for each id */
  short sorted_id[JV3_SECSMAX + 1];
  short track_start[MAXTRACKS][JV3_SIDES];
#else
  SectorId id[1];   /* extra one is a loop sentinel */
  int offset[1];    /* offset into file for each id */
  short sorted_id[1];
  short track_start[1][1];
#endif
} JV3State;

typedef struct {
  int rps;                        /* phys rotations/sec; emutype REAL only */
  int size_code;                  /* most recent sector size; REAL only */
  int empty;                      /* 1=emulate empty drive */
  time_t empty_timeout;           /* real_empty valid until this time */
  unsigned int fmt_nbytes;        /* number of PC format command bytes */
  int fmt_fill;                   /* fill byte for data sectors */
  Uint8 buf[MAXSECSIZE];
} RealState;

/* Some constants for DMK format */
#define DMK_WRITEPROT     0
#define DMK_NTRACKS       1
#define DMK_TRACKLEN      2
#define DMK_TRACKLEN_SIZE 2
#define DMK_OPTIONS       4
#define DMK_FORMAT        0x0c
#define DMK_FORMAT_SIZE   4
#define DMK_HDR_SIZE      0x10
#define DMK_TKHDR_SIZE    0x80    /* Space reserved for IDAM pointers */
#define DMK_TRACKLEN_MAX  0x4000

/* Bit assignments in options */
#define DMK_SSIDE_OPT     0x10
#define DMK_SDEN_OPT      0x40
#define DMK_IGNDEN_OPT    0x80

/* Bit assignments in IDAM pointers */
#define DMK_DDEN_FLAG     0x8000
#define DMK_EXTRA_FLAG    0x4000  /* unused */
#define DMK_IDAMP_BITS    0x3fff

#define dmk_incr(d) \
  (((d)->u.dmk.ignden || (d)->u.dmk.sden || state.density) ? 1 : 2)

typedef struct {
  int ntracks;                    /* max number of tracks formatted */
  int tracklen;                   /* bytes reserved per track in file */
  int nsides;                     /* 1 or 2 (single-sided flag in header) */
  int sden;                       /* single-density-only flag in header */
  int ignden;                     /* ignore-density flag in header */
  int curtrack, curside;          /* track/side in track buffer, or -1/-1 */
  int curbyte;                    /* index in buf for current op */
  int nextidam;                   /* index in buf to put next idam */
  Uint8 buf[DMK_TRACKLEN_MAX];
} DMKState;

typedef struct {
  int writeprot;		  /* emulated write protect tab */
  int phytrack;			  /* where are we really? */
  int emutype;
  int inches;                     /* 5 or 8, as seen by TRS-80 */
  int real_step;                  /* 1=normal, 2=double-step if REAL */
  FILE* file;
  char filename[FILENAME_MAX];
  union {
    JV3State jv3;                 /* valid if emutype = JV3 */
    RealState real;               /* valid if emutype = REAL */
    DMKState dmk;                 /* valid if emutype = DMK */
  } u;
} DiskState;

static DiskState disk[NDRIVES] EXT_RAM_ATTR;

/* Emulate interleave in JV1 mode */
static const Uint8 jv1_interleave[10] = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};

/* Forward */
static void real_verify(void);
static void real_restore(int curdrive);
static void real_seek(void);
static void real_read(void);
static void real_write(void);
static void real_readadr(void);
static void real_readtrk(void);
static void real_writetrk(void);
static int  real_check_empty(DiskState *d);

/********************************************************************************************
 * TRS-IO compat
 ********************************************************************************************/

typedef void (*trs_event_func)(int arg);

static const int trs_model = 1;

static const int eg3200 = 0;

static const Uint16 Z80_PC = 0;

static const int trs_show_led = 0;

static struct {
  int clockMHz;
  long long t_count;
  long long sched;
} z80_state;

#define TSTATE_T_MID 0

static void error(const char* fmt, ...)
{
  // Do nothing
}

static void warn(const char* fmt, ...)
{
  // Do nothing
}

static void debug(const char* fmt, ...)
{
  // Do nothing
}

void trs_disk_led(int drive, int on_off)
{
  // Do nothing
}

void trs_schedule_event(trs_event_func f, int arg, int tstates)
{
  assert(0);
}

void trs_do_event()
{
  assert(0);
}

void trs_disk_motoroff_interrupt(int arg)
{
  assert(0);
}

void trs_disk_intrq_interrupt(int arg)
{
  assert(0);
}

void trs_disk_drq_interrupt(int arg)
{
  assert(0);
}

trs_event_func trs_event_scheduled()
{
  return NULL;
}

void trs_cancel_event()
{
  //assert(0);
}


/* Entry point for the zbx debugger */
void
trs_disk_debug(void)
{
#if 0
  int i;
  puts("Floppy disk controller state:");
  printf("  status 0x%02x, track %d (0x%02x), sector %d (0x%02x), "
	 "data 0x%02x\n", state.status, state.track, state.track,
	 state.sector, state.sector, state.data);
  printf("  currcommand 0x%02x, bytecount left %d, last step direction %d\n",
	 state.currcommand, state.bytecount, state.lastdirection);
  printf("  curdrive %d, curside %d, density %d, controller %s\n",
	 state.curdrive, state.curside, state.density,
	 state.controller == TRSDISK_P1771 ? "WD1771" : "WD1791/93");
  printf("  crc state 0x%04x, last_readadr %d, motor timeout %ld\n",
	 state.crc, state.last_readadr,
	 (long) (state.motor_timeout - z80_state.t_count));
  printf("  last (non-DMK) format gaps %d %d %d %d %d\n",
	 state.format_gap[0], state.format_gap[1], state.format_gap[2],
	 state.format_gap[3], state.format_gap[4]);
  printf("  debug flags: %#x\n", trs_disk_debug_flags);
  for (i = 0; i < NDRIVES; i++) {
    DiskState *d = &disk[i];
    printf("Drive %d state: "
	   "writeprot %d, phytrack %d (0x%02x), inches %d, step %d, type ",
	   i, d->writeprot, d->phytrack, d->phytrack, d->inches, d->real_step);
    if (d->file == NULL) {
      puts("EMPTY");
    } else {
      switch (d->emutype) {
      case JV1:
	puts("JV1");
	break;
      case JV3:
	puts("JV3");
	printf("  last used id %d, id blocks %d\n",
	       d->u.jv3.last_used_id, d->u.jv3.nblocks);
	break;
      case DMK:
	puts("DMK");
	printf("  ntracks %d (0x%02x), tracklen 0x%04x, nsides %d, sden %d, "
	       "ignden %d\n", d->u.dmk.ntracks, d->u.dmk.ntracks,
	       d->u.dmk.tracklen, d->u.dmk.nsides, d->u.dmk.sden,
	       d->u.dmk.ignden);
	printf("  buffered track %d, side %d, curbyte %d, nextidam %d\n",
	       d->u.dmk.curtrack, d->u.dmk.curside, d->u.dmk.curbyte,
	       d->u.dmk.nextidam);
	break;
      case REAL:
	puts("REAL");
	printf("  rpm %d, empty %d, last size code %d, last fmt fill 0x%02x\n",
	       d->u.real.rps * 60, d->u.real.empty, d->u.real.size_code,
	       d->u.real.fmt_fill);
	break;
      default:
	puts("UNKNOWN");
	break;
      }
    }
  }
#endif
}

void
trs_disk_setsize(int unit, int value)
{
  if (unit < 0 || unit > 7) return;
  disk[unit].inches = (value == 8) ? 8 : 5;
}

#ifdef __linux
void
trs_disk_setstep(int unit, int value)
{
  if (unit < 0 || unit > 7) return;
  disk[unit].real_step = (value == 2) ? 2 : 1;
}
#endif

int
trs_disk_getsize(int unit)
{
  if (unit < 0 || unit > 7) return 0;
  return disk[unit].inches;
}

char*
trs_disk_getfilename(int unit)
{
  return disk[unit].filename;
}

int
trs_disk_getdisktype(int unit)
{
  return disk[unit].emutype;
}

int
trs_disk_getwriteprotect(int unit)
{
  return disk[unit].writeprot;
}

#ifdef __linux
int
trs_disk_getstep(int unit)
{
  if (unit < 0 || unit > 7) return 0;
  return disk[unit].real_step;
}
#endif

void
trs_disk_init(int poweron)
{
  int i;

  state.status = TRSDISK_NOTRDY|TRSDISK_TRKZERO;
  state.track = 0;
  state.sector = 0;
  state.data = 0;
  state.currcommand = TRSDISK_RESTORE;
  state.lastdirection = 1;
  state.bytecount = 0;
  state.format = FMT_DONE;
  state.format_bytecount = 0;
  state.format_sec = 0;
  state.curdrive = state.curside = 0;
  state.density = 0;
  state.controller = (trs_model == 1) ? TRSDISK_P1771 : TRSDISK_P1791;
  state.last_readadr = -1;
  state.motor_timeout = 0;
  if (poweron) {
    for (i = 0; i < NDRIVES; i++) {
      disk[i].phytrack = 0;
      if (disk[i].file == NULL) {
	  disk[i].emutype = NONE;
	  disk[i].filename[0] = 0;
      }
    }
  }
  //trs_hard_init(poweron);
  //stringy_init();
  trs_cancel_event();

/*
 * Emulate no controller if there is no disk in drive 0 at reset time,
 * except for model 4.
 */
  trs_disk_nocontroller = (trs_model < 5 && disk[0].file == NULL);
}

/* trs_event_func used for delayed command completion.  Clears BUSY,
   sets any additional bits specified, and generates a command
   completion interrupt */
void
trs_disk_done(int bits)
{
  state.status &= ~TRSDISK_BUSY;
  state.status |= bits;
  trs_disk_intrq_interrupt(1);
}


/* trs_event_func to abort the last command with LOSTDATA if it is
   still in progress */
void
trs_disk_lostdata(int cmd)
{
  if (state.currcommand == cmd) {
    state.status &= ~TRSDISK_BUSY;
    state.status |= TRSDISK_LOSTDATA;
    state.bytecount = 0;
    trs_disk_intrq_interrupt(1);
  }
}

/* trs_event_func used as a delayed command start.  Sets DRQ,
   generates a DRQ interrupt, sets any additional bits specified, and
   schedules a trs_disk_lostdata event. */
void
trs_disk_firstdrq(int bits)
{
  state.status |= TRSDISK_DRQ | bits;
  trs_disk_drq_interrupt(1);
  trs_schedule_event(trs_disk_lostdata, state.currcommand,
		     500000 * z80_state.clockMHz);
}

static void
trs_disk_unimpl(Uint8 cmd, char* more)
{
  state.status = TRSDISK_NOTRDY|TRSDISK_WRITEFLT|TRSDISK_NOTFOUND;
  state.bytecount = state.format_bytecount = 0;
  state.format = FMT_DONE;
  trs_disk_drq_interrupt(0);
  trs_schedule_event(trs_disk_done, 0, 0);
  error("trs_disk_command(0x%02x) not implemented - %s", cmd, more);
}

/* Sort first by track, second by side, third by position in emulated-disk
   sector array (i.e., physical sector order on track).  */
static int
jv3_id_compare(const void* p1, const void* p2)
{
  DiskState *d = &disk[state.curdrive];
  int i1 = *(short*)p1;
  int i2 = *(short*)p2;
  int r = d->u.jv3.id[i1].track - d->u.jv3.id[i2].track;
  if (r != 0) return r;
  r = (d->u.jv3.id[i1].flags & JV3_SIDE) - (d->u.jv3.id[i2].flags & JV3_SIDE);
  if (r != 0) return r;
  return i1 - i2;
}

/* (Re-)create the sorted_id data structure for the given drive */
static void
jv3_sort_ids(int drive)
{
  DiskState *d = &disk[drive];
  int olddrive = state.curdrive;
  int i, track, side;

  for (i = 0; i <= JV3_SECSMAX; i++) {
    d->u.jv3.sorted_id[i] = i;
  }
  state.curdrive = drive;
  qsort((void*) d->u.jv3.sorted_id, JV3_SECSMAX, sizeof(short),
	jv3_id_compare);
  state.curdrive = olddrive;

  for (track = 0; track < MAXTRACKS; track++) {
    d->u.jv3.track_start[track][0] = -1;
    d->u.jv3.track_start[track][1] = -1;
  }
  track = side = -1;
  for (i = 0; i < JV3_SECSMAX; i++) {
    SectorId *sid = &d->u.jv3.id[d->u.jv3.sorted_id[i]];
    if (sid->track != track ||
	(sid->flags & JV3_SIDE ? 1 : 0) != side) {
      track = sid->track;
      if (track == JV3_FREE) break;
      side = sid->flags & JV3_SIDE ? 1 : 0;
      d->u.jv3.track_start[track][side] = i;
    }
  }

  d->u.jv3.sorted_valid = 1;
}

/* JV3 only */
static int
id_index_to_size_code(DiskState *d, int id_index)
{
  return (d->u.jv3.id[id_index].flags & JV3_SIZE) ^
    ((d->u.jv3.id[id_index].track == JV3_FREE) ? 2 : 1);
}

/* IBM formats only */
static int
size_code_to_size(int code)
{
  return 128 << code;
}

/* JV3 only */
static int
id_index_to_size(DiskState *d, int id_index)
{
  return 128 << id_index_to_size_code(d, id_index);
}

/* Return the offset of the data block for the id_index'th sector
   in an emulated-disk file.  Not used for DMK. */
static off_t
offset(DiskState *d, int id_index)
{
  if (d->emutype == JV1) {
    return id_index * JV1_SECSIZE;
  } else if (d->emutype == JV3) {
    return d->u.jv3.offset[id_index];
  } else {
    trs_disk_unimpl(state.currcommand, "DMK offset (internal error)");
    return 0;
  }
}

/* Return the offset of the id block for the id_index'th sector
   in an emulated-disk file.  Initialize a new block if needed.  JV3 only. */
static off_t
idoffset(DiskState *d, int id_index)
{
  if (d->emutype == JV1 || d->emutype == DMK) {
    trs_disk_unimpl(state.currcommand, "JV1 or DMK idoffset (internal error)");
    return -1;
  } else {
    if (id_index < JV3_SECSPERBLK) {
      return JV3_IDSTART + id_index * sizeof(SectorId);
    } else {
      int idstart2 = d->u.jv3.offset[JV3_SECSPERBLK-1] +
	id_index_to_size(d, JV3_SECSPERBLK-1);

      if (d->u.jv3.nblocks == 1) {
        /* Initialize new block of ids */
	int c;
	fseek(d->file, idstart2, 0);
        c = fwrite((void*)&d->u.jv3.id[JV3_SECSPERBLK], JV3_SECSTART, 1, d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	d->u.jv3.nblocks = 2;
      }
      return idstart2 + (id_index - JV3_SECSPERBLK) * sizeof(SectorId);
    }
  }
}

static int
jv3_alloc_sector(DiskState *d, int size_code)
{
  int maybe = d->u.jv3.free_id[size_code];
  d->u.jv3.sorted_valid = 0;
  while (maybe <= d->u.jv3.last_used_id) {
    if (d->u.jv3.id[maybe].track == JV3_FREE &&
	id_index_to_size_code(d, maybe) == size_code) {
      d->u.jv3.free_id[size_code] = maybe + 1;
      return maybe;
    }
    maybe++;
  }
  d->u.jv3.free_id[size_code] = JV3_SECSMAX; /* none are free */
  if (d->u.jv3.last_used_id >= JV3_SECSMAX-1) {
    return -1;
  }
  d->u.jv3.last_used_id++;
  d->u.jv3.offset[d->u.jv3.last_used_id + 1] =
    d->u.jv3.offset[d->u.jv3.last_used_id] + size_code_to_size(size_code);
  if (d->u.jv3.last_used_id + 1 == JV3_SECSPERBLK) {
      d->u.jv3.offset[d->u.jv3.last_used_id + 1] += JV3_SECSTART;
  }
  return d->u.jv3.last_used_id;
}

static void
jv3_free_sector(DiskState *d, int id_index)
{
  int c;
  int size_code = (d->u.jv3.id[id_index].flags & JV3_SIZE) ^ 1;
  if (d->u.jv3.free_id[size_code] > id_index) {
    d->u.jv3.free_id[size_code] = id_index;
  }
  d->u.jv3.sorted_valid = 0;
  d->u.jv3.id[id_index].track = JV3_FREE;
  d->u.jv3.id[id_index].sector = JV3_FREE;
  d->u.jv3.id[id_index].flags =
    (d->u.jv3.id[id_index].flags | JV3_FREEF) ^ JV3_SIZE;
  fseek(d->file, idoffset(d, id_index), 0);
  c = fwrite(&d->u.jv3.id[id_index], sizeof(SectorId), 1, d->file);
  if (c == EOF) state.status |= TRSDISK_WRITEFLT;

  if (id_index == d->u.jv3.last_used_id) {
    int newlen;
    while (d->u.jv3.id[d->u.jv3.last_used_id].track == JV3_FREE) {
      d->u.jv3.last_used_id--;
    }
    c = fflush(d->file);
    if (c == EOF) state.status |= TRSDISK_WRITEFLT;
    rewind(d->file);
    if (d->u.jv3.last_used_id >= 0) {
      newlen = offset(d, d->u.jv3.last_used_id) +
	id_index_to_size(d, d->u.jv3.last_used_id);
    } else {
      newlen = offset(d, 0);
    }
#ifdef _WIN32
    chsize(fileno(d->file), newlen);
#else
    c = ftruncate(fileno(d->file), newlen);
    if (c == EOF) state.status |= TRSDISK_WRITEFLT;
#endif
  }
}

/* Heuristic to decide what file format we have */
/* Also decodes write-protect state */
static void
trs_disk_emutype(DiskState *d)
{
  int c;
  char fmt[4];
  int count;

  fseek(d->file, 0, 0);
  c = getc(d->file);
  if (c == -1) {
    d->emutype = JV1;
    return;
  }
  if (c == 0 || c == 0xff) {
    fseek(d->file, DMK_FORMAT, 0);
    count = fread(fmt, 1, DMK_FORMAT_SIZE, d->file);
    if (count != DMK_FORMAT_SIZE) {
      d->emutype = JV1;
      return;
    }
    if (fmt[0] == 0 && fmt[1] == 0 && fmt[2] == 0 && fmt[3] == 0) {
      fseek(d->file, DMK_TRACKLEN, 0);
      count = (Uint8) getc(d->file);
      count += (Uint8) getc(d->file) << 8;
      if (count >= 16 && count <= DMK_TRACKLEN_MAX) {
	d->emutype = DMK;
	d->writeprot = d->writeprot || (c == 0xff);
	return;
      }
    }
    if (fmt[0] == 0x78 && fmt[1] == 0x56 && fmt[2] == 0x34 && fmt[3] == 0x12) {
      error("Real disk specifier file from DMK emulator not supported");
      d->emutype = NONE;
      fclose(d->file);
      d->file = NULL;
      return;
    }
  }
  if (c == 0) {
    fseek(d->file, 1, 0);
    if (getc(d->file) == 0xfe) {
      d->emutype = JV1;
      return;
    }
  }
  fseek(d->file, JV3_SECSPERBLK*sizeof(SectorId), 0);
  c = getc(d->file);
  if (c == 0 || c == 0xff) {
    d->emutype = JV3;
    d->writeprot = d->writeprot || (c == 0);
    return;
  }
  d->emutype = JV1;
}

void
trs_disk_remove(int drive)
{
  DiskState *d = &disk[drive];

  if (d->file != NULL) {
    if (fclose(d->file) == EOF) state.status |= TRSDISK_WRITEFLT;
    d->file = NULL;
    d->filename[0] = 0;
  }
  d->writeprot = 0;
}

void
trs_disk_insert(int drive, const char *diskname)
{
  DiskState *d = &disk[drive];
  struct stat st;
  int c;

  if (d->file != NULL) {
    c = fclose(d->file);
    if (c == EOF) state.status |= TRSDISK_WRITEFLT;
  }
  if (stat(diskname, &st) == -1) {
    d->file = NULL;
    d->filename[0] = 0;
    d->writeprot = 0;
    error("failed to open disk image %s: %s", diskname, strerror(errno));
    return;
  }
  #if __linux
  if (S_ISBLK(st.st_mode)) {
    /* Real floppy drive */
    int fd;
    int reset_now = 0;
    struct floppy_drive_params fdp;
    fd = open(diskname, O_ACCMODE|O_NDELAY);
    if (fd == -1) {
      error("%s: %s", diskname, strerror(errno));
      d->file = NULL;
      d->filename[0] = 0;
      d->emutype = JV3;
      return;
    }
    d->file = fdopen(fd, "r+");
    if (d->file == NULL) {
      error("%s: %s", diskname, strerror(errno));
      d->filename[0] = 0;
      d->emutype = JV3;
      return;
    }
    d->writeprot = 0;
    ioctl(fileno(d->file), FDRESET, &reset_now);
    ioctl(fileno(d->file), FDGETDRVPRM, &fdp);
    d->u.real.rps = fdp.rps;
    d->u.real.size_code = 1; /* initial guess: 256 bytes */
    d->u.real.empty_timeout = 0;
    if (d->emutype != REAL) {
      d->emutype = REAL;
      d->phytrack = 0;
      real_restore(drive);
    }
    snprintf(d->filename, FILENAME_MAX, "%s", diskname);
  } else
#endif
  {
    d->file = fopen(diskname, "rb+");
    if (d->file == NULL) {
      d->file = fopen(diskname, "rb");
      if (d->file == NULL) return;
      d->writeprot = 1;
    } else {
      d->writeprot = 0;
    }
    trs_disk_emutype(d);
    snprintf(d->filename, FILENAME_MAX, "%s", diskname);
  }
  if (d->emutype == JV3) {
    int id_index, n;
    int ofst;

    memset((void*)d->u.jv3.id, JV3_FREE, sizeof(d->u.jv3.id));

    /* Read first block of ids */
    fseek(d->file, JV3_IDSTART, 0);
    n = fread((void*)&d->u.jv3.id[0], 3, JV3_SECSPERBLK, d->file);

    /* Scan to find their offsets */
    ofst = JV3_SECSTART;
    for (id_index = 0; id_index < JV3_SECSPERBLK; id_index++) {
      d->u.jv3.offset[id_index] = ofst;
      ofst += id_index_to_size(d, id_index);
    }

    /* Read second block of ids, if any */
    fseek(d->file, ofst, 0);
    n = fread((void*)&d->u.jv3.id[JV3_SECSPERBLK], 3, JV3_SECSPERBLK, d->file);
    d->u.jv3.nblocks = n > 0 ? 2 : 1;

    /* Scan to find their offsets */
    ofst += JV3_SECSTART;
    for (id_index = JV3_SECSPERBLK; id_index < JV3_SECSPERBLK*2; id_index++) {
      d->u.jv3.offset[id_index] = ofst;
      ofst += id_index_to_size(d, id_index);
    }

    /* Find u.jv3.last_used_id value and u.jv3.free_id hints */
    for (n = 0; n < 4; n++) {
      d->u.jv3.free_id[n] = JV3_SECSMAX;
    }
    d->u.jv3.last_used_id = -1;
    for (id_index = 0; id_index < JV3_SECSMAX; id_index++) {
      if (d->u.jv3.id[id_index].track == JV3_FREE) {
	int size_code = id_index_to_size_code(d, id_index);
	if (d->u.jv3.free_id[size_code] == JV3_SECSMAX) {
	  d->u.jv3.free_id[size_code] = id_index;
	}
      } else {
	d->u.jv3.last_used_id = id_index;
      }
    }
    jv3_sort_ids(drive);
  } else if (d->emutype == DMK) {
    fseek(d->file, DMK_NTRACKS, 0);
    d->u.dmk.ntracks = (Uint8) getc(d->file);
    d->u.dmk.tracklen = (Uint8) getc(d->file);
    d->u.dmk.tracklen += ((Uint8) getc(d->file)) << 8;
    c = getc(d->file);
    d->u.dmk.nsides = (c & DMK_SSIDE_OPT) ? 1 : 2;
    d->u.dmk.sden = (c & DMK_SDEN_OPT) != 0;
    d->u.dmk.ignden = (c & DMK_IGNDEN_OPT) != 0;
    d->u.dmk.curtrack = d->u.dmk.curside = -1;

    if (trs_disk_debug_flags & DISKDEBUG_DMK) {
      debug("DMK drv=%d wp=%d #tk=%d tklen=0x%x nsides=%d sden=%d ignden=%d\n",
	    drive, d->writeprot, d->u.dmk.ntracks, d->u.dmk.tracklen,
	    d->u.dmk.nsides, d->u.dmk.sden, d->u.dmk.ignden);
    }
  }
}

static int
cmd_type(Uint8 cmd)
{
  switch (cmd & TRSDISK_CMDMASK) {
  case TRSDISK_RESTORE:
  case TRSDISK_SEEK:
  case TRSDISK_STEP:
  case TRSDISK_STEPU:
  case TRSDISK_STEPIN:
  case TRSDISK_STEPINU:
  case TRSDISK_STEPOUT:
  case TRSDISK_STEPOUTU:
    return 1;
  case TRSDISK_READ:
  case TRSDISK_READM:
  case TRSDISK_WRITE:
  case TRSDISK_WRITEM:
    return 2;
  case TRSDISK_READADR:
  case TRSDISK_READTRK:
  case TRSDISK_WRITETRK:
    return 3;
  case TRSDISK_FORCEINT:
    return 4;
  }
  return -1;			/* not reached */
}


/* Called by the interrupt code to determine whether a motoroff NMI is
   required.  Called even if this NMI is masked, so we also use it here
   to set NOTRDY and LOSTDATA. */
int
trs_disk_motoroff(void)
{
  int stopped;
  int cmdtype;

  stopped = (state.motor_timeout - z80_state.t_count > TSTATE_T_MID);
  if (stopped) {
    state.status |= TRSDISK_NOTRDY;
    cmdtype = cmd_type(state.currcommand);
    if ((cmdtype == 2 || cmdtype == 3) && (state.status & TRSDISK_DRQ)) {
      /* Also end the command and set Lost Data for good measure */
      state.status = (state.status | TRSDISK_LOSTDATA) &
	~(TRSDISK_BUSY | TRSDISK_DRQ);
      state.bytecount = 0;
    }
  }
  return stopped;
}

/* Get the on-disk track data from the current track/side into the buffer */
static void
dmk_get_track(DiskState* d)
{
  if (d->phytrack == d->u.dmk.curtrack &&
      state.curside == d->u.dmk.curside) return;
  d->u.dmk.curtrack = d->phytrack;
  d->u.dmk.curside = state.curside;
  if (d->u.dmk.curtrack >= d->u.dmk.ntracks ||
      (d->u.dmk.curside && d->u.dmk.nsides == 1)) {
    memset(d->u.dmk.buf, 0, sizeof(d->u.dmk.buf));
    return;
  }
  fseek(d->file, (DMK_HDR_SIZE +
		  (d->u.dmk.curtrack * d->u.dmk.nsides + d->u.dmk.curside)
		  * d->u.dmk.tracklen), 0);
  if (fread(d->u.dmk.buf, d->u.dmk.tracklen, 1, d->file) != 1) {
    memset(d->u.dmk.buf, 0, sizeof(d->u.dmk.buf));
    return;
  }
}


/* Search for a sector on the current physical track.  For JV1 or JV3,
   return its index within the emulated disk's array of sectors.  For
   DMK, get the track into the buffer, return the index of the next
   byte after the header CRC, and set state.bytecount to its size
   code.  Set status and return -1 if there is no such sector.  If
   sector == -1, return the first sector found if any.  If side == 0
   or 1, perform side compare against sector ID; if -1, don't. */
static int
search(int sector, int side)
{
  DiskState *d = &disk[state.curdrive];
  if (d->file == NULL) {
    state.status |= TRSDISK_NOTFOUND;
    return -1;
  }
  if (d->emutype == JV1) {
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside > 0 || sector >= JV1_SECPERTRK || d->file == NULL ||
	d->phytrack != state.track || state.density == 1 || side == 1) {
      state.status |= TRSDISK_NOTFOUND;
      return -1;
    }
    return JV1_SECPERTRK * d->phytrack + (sector < 0 ? 0 : sector);
  } else if (d->emutype == JV3) {
    int i;
    SectorId *sid;
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside >= JV3_SIDES ||
	(side != -1 && side != state.curside) ||
	d->phytrack != state.track || d->file == NULL) {
      state.status |= TRSDISK_NOTFOUND;
      return -1;
    }
    if (!d->u.jv3.sorted_valid) jv3_sort_ids(state.curdrive);
    i = d->u.jv3.track_start[d->phytrack][state.curside];
    if (i != -1) {
      for (;;) {
	sid = &d->u.jv3.id[d->u.jv3.sorted_id[i]];
	if (sid->track != d->phytrack ||
	    (sid->flags & JV3_SIDE ? 1 : 0) != state.curside) break;
	if ((sector == -1 || sid->sector == sector) &&
	    ((sid->flags & JV3_DENSITY) ? 1 : 0) == state.density) {
	  return d->u.jv3.sorted_id[i];
	}
	i++;
      }
    }
    state.status |= TRSDISK_NOTFOUND;
    return -1;
  } else /* d->emutype == DMK */ {
    /* !!maybe someday start at a point determined by angle() and wrap
       back.  would deal more realistically with disks that have more
       than one of the same sector. */
    int i;
    int incr = dmk_incr(d);

    /* get current phytrack into buffer */
    dmk_get_track(d);

    /* loop through IDAMs in track */
    for (i = 0; i < DMK_TKHDR_SIZE; i += 2) {
      Uint8 *p;

      /* fetch index of next IDAM */
      int idamp = d->u.dmk.buf[i] + (d->u.dmk.buf[i + 1] << 8);

      /* fail if no more IDAMs */
      if (idamp == 0) break;

      /* skip IDAM if wrong density */
      if (!d->u.dmk.ignden &&
	  state.density != ((idamp & DMK_DDEN_FLAG) != 0)) continue;

      /* point p to IDAM */
      idamp &= DMK_IDAMP_BITS;
      p = &d->u.dmk.buf[idamp];

      /* fail if IDAM out of range */
      if (idamp >= DMK_TRACKLEN_MAX) break;

      /* initialize ID CRC */
      state.crc = state.density ? 0xcdb4 /* CRC of a1 a1 a1 */ : 0xffff;

      /* sanity check; is this an IDAM at all? */
      if (*p != 0xfe) continue;
      state.crc = calc_crc(state.crc, *p);
      p += incr;

      /* compare track field of ID */
      if (*p != state.track) continue;
      state.crc = calc_crc(state.crc, *p);
      p += incr;

      /* compare side field of ID if desired */
      if ((*p & 1) != side && side != -1) continue;
      state.crc = calc_crc(state.crc, *p);
      p += incr;

      /* compare sector field of ID if desired */
      if (*p != sector && sector != -1) continue;
      state.crc = calc_crc(state.crc, *p);
      p += incr;

      /* save size code field of ID; caller converts to actual byte count */
      state.bytecount = *p;
      state.crc = calc_crc(state.crc, *p);
      p += incr;

      /* fold CRC field into computation; result should be 0 */
      state.crc = calc_crc(state.crc, *p);
      p += incr;
      state.crc = calc_crc(state.crc, *p);
      p += incr;

      if (state.crc != 0) {
	/* set CRC error flag and look for another ID that matches */
	state.status |= TRSDISK_CRCERR;
	continue;
      } else {
	/* clear CRC error flag in case set for an earlier ID match */
	state.status &= ~TRSDISK_CRCERR;
      }

      /* Found an ID that matches */
      d->u.dmk.nextidam = i + 2; /* remember where the next one is */
      return p - d->u.dmk.buf;
    }
    state.status |= TRSDISK_NOTFOUND;
    return -1;
  }
}


/* Search for the first sector on the current physical track (in
   either density) and return its index within the sorted index array
   (JV3), or index within the sector array (JV1).  Not used for DMK.
   Return -1 if there is no such sector, or if reading JV1 in double
   density.  Don't set TRSDISK_NOTFOUND; leave the caller to do
   that. */
static int
search_adr(void)
{
  DiskState *d = &disk[state.curdrive];
  if (d->file == NULL) {
    return -1;
  }
  if (d->emutype == JV1) {
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside > 0 || d->file == NULL || state.density == 1) {
      return -1;
    }
    return JV1_SECPERTRK * d->phytrack;
  } else {
    if (d->phytrack < 0 || d->phytrack >= MAXTRACKS ||
	state.curside >= JV3_SIDES || d->file == NULL) {
      return -1;
    }
    if (!d->u.jv3.sorted_valid) jv3_sort_ids(state.curdrive);
    return d->u.jv3.track_start[d->phytrack][state.curside];
  }
}


static void
verify(void)
{
  /* Verify that head is on the expected track */
  DiskState *d = &disk[state.curdrive];
  if (d->emutype == REAL) {
    real_verify();
  } else if (d->emutype == JV1) {
    if (d->file == NULL) {
      state.status |= TRSDISK_NOTFOUND;
    } if (state.density == 1) {
      state.status |= TRSDISK_NOTFOUND;
    } else if (state.track != d->phytrack) {
      state.status |= TRSDISK_SEEKERR;
    }
  } else {
    search(-1, -1); /* TRSDISK_SEEKERR == TRSDISK_NOTFOUND */
  }
}

/* Return a value in [0,1) indicating how far we've rotated
 * from the leading edge of the index hole */
static float
angle(void)
{
  DiskState *d = &disk[state.curdrive];
  float a;
  /* Set revus to number of microseconds per revolution */
  int revus = d->inches == 5 ? 200000 /* 300 RPM */ : 166666 /* 360 RPM */;
#if TSTATEREV
  /* Lock revolution rate to emulated time measured in T-states */
  /* Minor bug: there will be a glitch when t_count wraps around on
     a 32-bit machine */
  int revt = (int)(revus * z80_state.clockMHz);
  a = ((float)(z80_state.t_count % revt)) / ((float)revt);
#else
  /* Old way: lock revolution rate to real time */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  /* Ignore the seconds field; this is OK if there are a round number
     of revolutions per second */
  a = ((float)(tv.tv_usec % revus)) / ((float)revus);
#endif
  return a;
}

static void
type1_status(void)
{
  DiskState *d = &disk[state.curdrive];

  switch (cmd_type(state.currcommand)) {
  case 1:
  case 4:
    break;
  default:
    return;
  }

  if (d->file == NULL || (d->emutype == REAL && d->u.real.empty)) {
    state.status |= TRSDISK_INDEX;
  } else {
    if (angle() < trs_disk_holewidth) {
      state.status |= TRSDISK_INDEX;
    } else {
      state.status &= ~TRSDISK_INDEX;
    }
    if (d->writeprot) {
      state.status |= TRSDISK_WRITEPRT;
    } else {
      state.status &= ~TRSDISK_WRITEPRT;
    }
  }
  if (d->phytrack == 0) {
    state.status |= TRSDISK_TRKZERO;
  } else {
    state.status &= ~TRSDISK_TRKZERO;
  }
  /* RDY and HLT inputs are wired together on TRS-80 I/III/4/4P */
  if (state.status & TRSDISK_NOTRDY) {
    state.status &= ~TRSDISK_HEADENGD;
  } else {
    state.status |= TRSDISK_HEADENGD;
  }
}

void
trs_disk_select_write(Uint8 data)
{
  static int old_data = -1;

  if ((trs_disk_debug_flags & DISKDEBUG_FDCREG) && data != old_data) {
    //debug("select_write(0x%02x) pc 0x%04x\n", data, Z80_PC);
    old_data = data;
  }

  state.status &= ~TRSDISK_NOTRDY;
  /* EG 3200 uses bit 4 to select side */
  if (eg3200) {
    state.curside = (data & 0x10) != 0;
  } else if (trs_model == 1) {
    /* Disk 3 and side select share a bit.  You can't have a drive :3
       on a real Model I if any drive is two-sided.  Here we are more
       generous and just forbid drive :3 from being 2-sided. */
    state.curside = ( (data & (TRSDISK_0|TRSDISK_1|TRSDISK_2)) != 0 &&
		      (data & TRSDISK_SIDE) != 0 );
    if (state.curside) data &= ~TRSDISK_SIDE;
  } else {
    state.curside = (data & TRSDISK3_SIDE) != 0;
    state.density = (data & TRSDISK3_MFM) != 0;
    if (data & TRSDISK3_WAIT) {
      /* If there was an event pending, simulate waiting until
	 it was due. */
      if (trs_event_scheduled() != NULL &&
	  trs_event_scheduled() != trs_disk_lostdata) {
	z80_state.t_count = z80_state.sched;
	trs_do_event();
      }
    }
  }
  switch (data & (TRSDISK_0|TRSDISK_1|TRSDISK_2|TRSDISK_3)) {
  case 0:
    state.status |= TRSDISK_NOTRDY;
    break;
  case TRSDISK_0:
    state.curdrive = 0;
    break;
  case TRSDISK_1:
    state.curdrive = 1;
    break;
  case TRSDISK_2:
    state.curdrive = 2;
    break;
  case TRSDISK_3:
    state.curdrive = 3;
    break;
  case TRSDISK_4:
    /* fake value for emulator only */
    state.curdrive = 4;
    break;
  case TRSDISK_5:
    /* fake value for emulator only */
    state.curdrive = 5;
    break;
  case TRSDISK_6:
    /* fake value for emulator only */
    state.curdrive = 6;
    break;
  case TRSDISK_7:
    /* fake value for emulator only */
    state.curdrive = 7;
    break;
  default:
    trs_disk_unimpl(data, "bogus drive select");
    state.status |= TRSDISK_NOTRDY;
    break;
  }

  /* If a drive was selected... */
  if (!(state.status & TRSDISK_NOTRDY)) {
    DiskState *d = &disk[state.curdrive];

    /* Retrigger emulated motor timeout */
    state.motor_timeout = z80_state.t_count +
      MOTOR_USEC * z80_state.clockMHz;
    trs_disk_motoroff_interrupt(0);

    /* Update our knowledge of whether there is a real disk present */
    if (d->emutype == REAL) real_check_empty(d);
  }
}

Uint8
trs_disk_track_read(void)
{
  if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
    //debug("track_read() => 0x%02x pc 0x%04x\n", state.track, Z80_PC);
  }
  return state.track;
}

void
trs_disk_track_write(Uint8 data)
{
  if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
    //debug("track_write(0x%02x) pc 0x%04x\n", data, Z80_PC);
  }
  state.track = data;
}

Uint8
trs_disk_sector_read(void)
{
  if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
  }
  return state.sector;
}

void
trs_disk_set_controller(int controller)
{
  /* Support for more accurate Doubler emulation */
  FDCState tmp_state;
  if (state.controller == controller) return;
  tmp_state.status = state.status;
  tmp_state.track = state.track;
  tmp_state.sector = state.sector;
  tmp_state.data = state.data;
  tmp_state.lastdirection = state.lastdirection;
  state.controller = controller;
  state.status = other_state.status;
  state.track = other_state.track;
  state.sector = other_state.sector;
  state.data = other_state.data;
  state.lastdirection = other_state.lastdirection;
  other_state.status = tmp_state.status;
  other_state.track = tmp_state.track;
  other_state.sector = tmp_state.sector;
  other_state.data = tmp_state.data;
  other_state.lastdirection = tmp_state.lastdirection;
}

void
trs_disk_sector_write(Uint8 data)
{
  if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
    debug("sector_write(0x%02x) pc 0x%04x\n", data, Z80_PC);
  }
  if (trs_model == 1 && (trs_disk_doubler & TRSDISK_TANDY)) {
    switch (data) {
      /* Emulate Radio Shack doubler */
    case TRSDISK_R1791:
      trs_disk_set_controller(TRSDISK_P1791);
      state.density = 1;
      break;
    case TRSDISK_R1771:
      trs_disk_set_controller(TRSDISK_P1771);
      state.density = 0;
      break;
    case TRSDISK_NOPRECMP:
    case TRSDISK_PRECMP:
      /* Nothing for emulator to do */
      break;
    default:
      break;
    }
  }
  state.sector = data;
}

Uint8
trs_disk_data_read(void)
{
  DiskState *d = &disk[state.curdrive];
  SectorId *sid;

  if (trs_show_led)
    trs_disk_led(state.curdrive, 1);

  switch (state.currcommand & TRSDISK_CMDMASK) {

  case TRSDISK_READ:
    if (state.bytecount > 0 && (state.status & TRSDISK_DRQ)) {
      int c;
      if (d->emutype == REAL) {
	c = d->u.real.buf[size_code_to_size(d->u.real.size_code)
			 - state.bytecount];
      } else if (d->emutype == DMK) {
	c = d->u.dmk.buf[d->u.dmk.curbyte];
	state.crc = calc_crc(state.crc, c);
	d->u.dmk.curbyte += dmk_incr(d);
      } else {
	c = getc(d->file);
	if (c == EOF) {
	  c = 0xe5;
	  if (d->emutype == JV1) {
	    state.status &= ~TRSDISK_RECTYPE;
	    state.status |= (state.controller == TRSDISK_P1771) ?
	      TRSDISK_1771_FB : TRSDISK_1791_FB;
	  }
	}
      }
      state.data = c;
      state.bytecount--;
      if (state.bytecount <= 0) {
	if (d->emutype == DMK) {
	  state.crc = calc_crc(state.crc, d->u.dmk.buf[d->u.dmk.curbyte]);
	  d->u.dmk.curbyte += dmk_incr(d);
	  state.crc = calc_crc(state.crc, d->u.dmk.buf[d->u.dmk.curbyte]);
	  if (state.crc != 0) {
	    state.status |= TRSDISK_CRCERR;
	  }
	}
	state.bytecount = 0;
	state.status &= ~TRSDISK_DRQ;
        trs_disk_drq_interrupt(0);
	if (trs_event_scheduled() == trs_disk_lostdata) {
	  trs_cancel_event();
	}
	trs_schedule_event(trs_disk_done, 0, 64);
      }
    }
    break;

  case TRSDISK_READADR:
    if (state.bytecount <= 0 || !(state.status & TRSDISK_DRQ)) {
      break;
    }

    if (d->emutype == REAL) {
      state.data = d->u.real.buf[6 - state.bytecount];

    } else if (d->emutype == DMK) {
      state.data = d->u.dmk.buf[d->u.dmk.curbyte];
      d->u.dmk.curbyte += dmk_incr(d);

    } else if (state.last_readadr >= 0) {
      if (d->emutype == JV1) {
	switch (state.bytecount) {
	case 6:
	  state.data = d->phytrack;
	  break;
	case 5:
	  state.data = 0;
	  break;
	case 4:
	  state.data = jv1_interleave[state.last_readadr % JV1_SECPERTRK];
	  break;
	case 3:
	  state.data = 0x01;  /* 256 bytes always */
	  break;
	case 2:
	case 1:
	  state.data = state.crc >> 8;
	  break;
	}
      } else if (d->emutype == JV3) {
	sid = &d->u.jv3.id[d->u.jv3.sorted_id[state.last_readadr]];
	switch (state.bytecount) {
	case 6:
	  state.data = sid->track;
	  break;
	case 5:
	  state.data = (sid->flags & JV3_SIDE) != 0;
	  break;
	case 4:
	  state.data = sid->sector;
	  break;
	case 3:
	  state.data =
	    id_index_to_size_code(d, d->u.jv3.sorted_id[state.last_readadr]);
	  break;
	case 2:
	case 1:
	  state.data = state.crc >> 8;
	  break;
	}
      }
    }

    /*
     * The 1771 data sheet says that Read Address writes the sector
     * address of the ID field into the sector register.  The 179x
     * data sheet says that Read Address writes the track address into
     * the sector register.  xtrs 4.9d and earlier assumed the 1771
     * data sheet was right and the 179x data sheet was wrong, so it
     * wrote the sector address into the sector register and the track
     * address into the track register.  This apparently was wrong:
     * Eric Dittman tells me that the 179x data sheet was correct, and
     * that his CP/M Plus, CP/M 2.2, and TurboDOS failed to boot in
     * xtrs because of the emulation error.  XXX I want to test this
     * on real hardware to verify the behavior, but right now I have
     * only a Model 4P, so I can't test 1771 behavior.  For now I am
     * changing the code to exactly match the data sheets, though I
     * now worry that maybe the actual 1771 behavior may match the
     * 179x data sheet.
     */
    if (state.bytecount ==
	(state.controller == TRSDISK_P1771 ? 4 /*sec*/ : 6 /*trk*/ )) {
      state.sector = state.data;
    }

    state.crc = calc_crc(state.crc, state.data);
    state.bytecount--;
    if (state.bytecount <= 0) {
      if (d->emutype == DMK && state.crc != 0) {
	state.status |= TRSDISK_CRCERR;
      }
      state.bytecount = 0;
      state.status &= ~TRSDISK_DRQ;
      trs_disk_drq_interrupt(0);
      if (trs_event_scheduled() == trs_disk_lostdata) {
	trs_cancel_event();
      }
      trs_schedule_event(trs_disk_done, 0, 64);
    }
    break;

  case TRSDISK_READTRK:
    /* assert(emutype == DMK) */
    if (!(state.status & TRSDISK_DRQ)) break;
    if (state.bytecount > 0) {
      state.data = d->u.dmk.buf[d->u.dmk.curbyte];
      d->u.dmk.curbyte += dmk_incr(d);
      state.bytecount = state.bytecount - 2 + state.density;
    }
    if (state.bytecount <= 0) {
      state.bytecount = 0;
      state.status &= ~TRSDISK_DRQ;
      trs_disk_drq_interrupt(0);
      if (trs_event_scheduled() == trs_disk_lostdata) {
	trs_cancel_event();
      }
      trs_schedule_event(trs_disk_done, 0, 64);
    }
    break;

  default:
    break;
  }
  if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
    debug("data_read() => 0x%02x pc 0x%04x\n", state.data, Z80_PC);
  }
  return state.data;
}

void
trs_disk_data_write(Uint8 data)
{
  DiskState *d = &disk[state.curdrive];
  int c;

  if (trs_show_led)
    trs_disk_led(state.curdrive, 1);

  if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
    debug("data_write(0x%02x) pc 0x%04x\n", data, Z80_PC);
  }
  switch (state.currcommand & TRSDISK_CMDMASK) {
  case TRSDISK_WRITE:
    if (state.bytecount > 0) {
      if (d->emutype == REAL) {
	d->u.real.buf[size_code_to_size(d->u.real.size_code)
		     - state.bytecount] = data;
	state.bytecount--;
	if (state.bytecount <= 0) {
	  real_write();
	}
	break;
      }
      c = putc(data, d->file);
      if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      if (d->emutype == DMK) {
	d->u.dmk.buf[d->u.dmk.curbyte++] = data;
	if (dmk_incr(d) == 2) {
	  d->u.dmk.buf[d->u.dmk.curbyte++] = data;
	  c = putc(data, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	}
	state.crc = calc_crc(state.crc, data);
      }
      state.bytecount--;
      if (state.bytecount <= 0) {
	if (d->emutype == DMK) {
	  int idamp, i, j;
	  c = state.crc >> 8;
	  d->u.dmk.buf[d->u.dmk.curbyte++] = c;
	  c = putc(c, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  if (dmk_incr(d) == 2) {
	    d->u.dmk.buf[d->u.dmk.curbyte++] = c;
	    c = putc(c, d->file);
	    if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  }
	  c = state.crc & 0xff;
	  d->u.dmk.buf[d->u.dmk.curbyte++] = c;
	  c = putc(c, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  if (dmk_incr(d) == 2) {
	    d->u.dmk.buf[d->u.dmk.curbyte++] = c;
	    c = putc(c, d->file);
	    if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  }
	  /* Check if we smashed one or more following IDAMs; can
	     happen with weird "protected" formats */
	  i = j = d->u.dmk.nextidam;
	  while (i < DMK_TKHDR_SIZE) {
	    idamp = (d->u.dmk.buf[i] + (d->u.dmk.buf[i + 1] << 8))
	      & DMK_IDAMP_BITS;
	    if (idamp != 0 && idamp != DMK_IDAMP_BITS &&
		d->u.dmk.curbyte /*!!+ erase shutoff slop?*/ > idamp) {
	      /* Yes, smashed this one */
	      i += 2;
	      if (trs_disk_debug_flags & DISKDEBUG_DMK) {
		debug("DMK smashed phytk %d physec %d\n", d->phytrack, i/2);
	      }
	    } else {
	      /* No, keep this one */
	      if (j == i) break; /* none were smashed; early exit */
	      d->u.dmk.buf[j++] = d->u.dmk.buf[i++];
	      d->u.dmk.buf[j++] = d->u.dmk.buf[i++];
	    }
	  }
	  if (j != i) {
	    /* Smashed at least one; rewrite the track header */
	    while (j < DMK_TKHDR_SIZE) {
	      d->u.dmk.buf[j++] = 0;
	    }
	    fseek(d->file, DMK_HDR_SIZE +
		  (d->phytrack * d->u.dmk.nsides + state.curside) *
		  d->u.dmk.tracklen, 0);
	    c = fwrite(d->u.dmk.buf, DMK_TKHDR_SIZE, 1, d->file);
	    if (c != 1) state.status |= TRSDISK_WRITEFLT;
	  }
	}
	state.bytecount = 0;
	state.status &= ~TRSDISK_DRQ;
        trs_disk_drq_interrupt(0);
	if (trs_event_scheduled() == trs_disk_lostdata) {
	  trs_cancel_event();
	}
	trs_schedule_event(trs_disk_done, 0, 64);
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
    }
    break;
  case TRSDISK_WRITETRK:
    state.bytecount = state.bytecount - 2 + state.density;
    if (d->emutype == DMK) {
      if (state.bytecount <= 0) {
	if (state.format != FMT_DONE) {
	  if (trs_disk_debug_flags & DISKDEBUG_DMK) {
	    debug("complete track format dens %d tk %d side %d\n",
		  state.density, d->phytrack, state.curside);
	  }
	  state.format = FMT_DONE;
	  state.status &= ~TRSDISK_DRQ;
	  /* Done: write modified track */
	  fseek(d->file, DMK_HDR_SIZE +
		(d->phytrack * d->u.dmk.nsides + state.curside) *
		d->u.dmk.tracklen, 0);
	  c = fwrite(d->u.dmk.buf, d->u.dmk.tracklen, 1, d->file);
	  if (c != 1) state.status |= TRSDISK_WRITEFLT;
	  if (d->phytrack >= d->u.dmk.ntracks) {
	    d->u.dmk.ntracks = d->phytrack + 1;
	    fseek(d->file, DMK_NTRACKS, 0);
	    putc(d->u.dmk.ntracks, d->file);
	  }
	  c = fflush(d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  trs_disk_drq_interrupt(0);
	  if (trs_event_scheduled() == trs_disk_lostdata) {
	    trs_cancel_event();
	  }
	  trs_schedule_event(trs_disk_done, 0, 64);
	}
      } else {
	switch (data) {
	case 0xf5:
	  if (state.density) {
	    data = 0xa1;
	    state.format = FMT_PREAM;
	    state.crc = 0x968b;  /* CRC of a1 a1 */
	  } else {
	    state.format = FMT_DATA;
	  }
	  break;
	case 0xf6:
	  if (state.density) {
	    data = 0xc2;
	    state.format = FMT_IPREAM;
	  } else {
	    state.format = FMT_DATA;
	  }
	  break;
	case 0xf7:
	  data = state.crc >> 8;
	  d->u.dmk.buf[d->u.dmk.curbyte++] = data;
	  if (dmk_incr(d) == 2) {
	    d->u.dmk.buf[d->u.dmk.curbyte++] = data;
	  }
	  state.bytecount = state.bytecount - 2 + state.density;
	  data = state.crc & 0xff;
	  state.format = FMT_DATA;
	  break;
	case 0xfe:
	  if (!state.density || state.format == FMT_PREAM) {
	    Uint16 idamp = d->u.dmk.curbyte +
	      (state.density ? DMK_DDEN_FLAG : 0);
	    if (d->u.dmk.nextidam >= DMK_TKHDR_SIZE) {
	      error("DMK formatting too many address marks on track");
	    } else if (d->u.dmk.curbyte > d->u.dmk.tracklen) {
	      error("DMK address mark past end of track");
	    } else {
	      d->u.dmk.buf[d->u.dmk.nextidam++] = idamp & 0xff;
	      d->u.dmk.buf[d->u.dmk.nextidam++] = idamp >> 8;
	    }
	  }
	  state.format = FMT_DATA;
	  if (!state.density) {
	    state.crc = 0xffff;
	  }
	  break;
#if DMK_MARK_IAM
	  /* Mark IAMs in the track header like IDAMs.  This turns
	     out to cause bogus errors when doing Read Address commands
	     both in current versions of David Keil's emulator and in
	     xtrs 4.5a and earlier, so we disable it, at least for now. */
	case 0xfc:
	  if (!state.density || state.format == FMT_IPREAM) {
	    Uint16 idamp = d->u.dmk.curbyte +
	      (state.density ? DMK_DDEN_FLAG : 0);
	    if (d->u.dmk.nextidam >= DMK_TKHDR_SIZE) {
	      error("DMK formatting too many address marks on track");
	    } else if (d->u.dmk.curbyte > d->u.dmk.tracklen) {
	      error("DMK address mark past end of track");
	    } else {
	      d->u.dmk.buf[d->u.dmk.nextidam++] = idamp & 0xff;
	      d->u.dmk.buf[d->u.dmk.nextidam++] = idamp >> 8;
	    }
	  }
	  state.format = FMT_DATA;
	  break;
#endif
	case 0xf8:
	case 0xf9:
	case 0xfa:
	case 0xfb:
	  if (!state.density) {
	    state.crc = 0xffff;
	  }
	  state.format = FMT_DATA;
	  break;
	default:
	  state.format = FMT_DATA;
	  break;
	}
	d->u.dmk.buf[d->u.dmk.curbyte++] = data;
	if (dmk_incr(d) == 2) {
	  d->u.dmk.buf[d->u.dmk.curbyte++] = data;
	}
	state.crc = calc_crc(state.crc, data);
      }
      break;
    }
    if (state.bytecount <= 0) {
      if (state.format == FMT_DONE) break;
      if (state.format == FMT_GAP2) {
	/* False ID: there was no DAM for following data */
	if (d->emutype != JV3) {
	  trs_disk_unimpl(state.currcommand, "false sector ID (no data)");
	} else {
	  /* We do not have a flag for this; try using CRC error */
	  d->u.jv3.id[state.format_sec].flags |= JV3_ERROR;
	  warn("recording false sector ID as CRC error");

	  /* Write the sector id */
	  fseek(d->file, idoffset(d, state.format_sec), 0);
	  c = fwrite(&d->u.jv3.id[state.format_sec],
		     sizeof(SectorId), 1, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	}
      } else if (state.format != FMT_GAP3) {
	/* If not in FMT_GAP3 state, format data was either too long,
	   had extra garbage following, or was intentionally non-
	   standard.  SuperUtility does a few tricks like "software
	   bulk erase" and duplication of protected disks, so we
	   do not complain about this any more. */
#if BOGUS
	error("format data end is not in gap4");
#endif
	state.format_gap[4] = 0;
      } else {
	/* This was really GAP4 */
	state.format_gap[4] = state.format_gapcnt;
	state.format_gapcnt = 0;
      }
      if (trs_disk_debug_flags & DISKDEBUG_GAPS) {
	debug("trk %d side %d gap0 %d gap1 %d gap2 %d gap3 %d gap4 %d\n",
	      d->phytrack, state.curside,
	      state.format_gap[0], state.format_gap[1], state.format_gap[2],
	      state.format_gap[3], state.format_gap[4]);
      }
      state.format = FMT_DONE;
      state.status &= ~TRSDISK_DRQ;
      if (d->emutype == REAL) {
	real_writetrk();
      } else {
	c = fflush(d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
      trs_disk_drq_interrupt(0);
      if (trs_event_scheduled() == trs_disk_lostdata) {
	trs_cancel_event();
      }
      trs_schedule_event(trs_disk_done, 0, 64);
      break;
    }
    switch (state.format) {
    case FMT_GAP0:
      if (data == 0xfc) {
	state.format = FMT_GAP1;
	state.format_gap[0] = state.format_gapcnt;
	state.format_gapcnt = 0;
      } else if (data == 0xfe) {
	/* There wasn't a gap 0; we were really in gap 1 */
	state.format_gap[0] = 0;
	goto got_idam;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_GAP1:
      if (data == 0xfe) {
      got_idam:
	/* We've received the first ID address mark */
	state.format_gap[1] = state.format_gapcnt;
	state.format_gapcnt = 0;
	state.format = FMT_TRACKID;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_GAP3:
      if (data == 0xfe) {
      got_idam2:
	/* We've received an ID address mark */
	state.format_gap[3] = state.format_gapcnt;
	state.format_gapcnt = 0;
	state.format = FMT_TRACKID;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_TRACKID:
      if (d->emutype == REAL) {
	if (d->u.real.fmt_nbytes >= sizeof(d->u.real.buf)) {
	  /* Data structure full */
	  state.status |= TRSDISK_WRITEFLT;
	  state.bytecount = 0;
	  state.format_bytecount = 0;
	  state.format = FMT_DONE;
	} else {
	  d->u.real.buf[d->u.real.fmt_nbytes++] = data;
	  state.format = FMT_HEADID;
	}
      } else {
	if (data != d->phytrack) {
	  trs_disk_unimpl(state.currcommand, "false track number");
	}
	state.format = FMT_HEADID;
      }
      break;
    case FMT_HEADID:
      if (d->emutype == REAL) {
	d->u.real.buf[d->u.real.fmt_nbytes++] = data;
      } else if (d->emutype == JV1) {
	if (data != 0) {
	  trs_disk_unimpl(state.currcommand, "JV1 double sided");
	}
	if (state.density) {
	  trs_disk_unimpl(state.currcommand, "JV1 double density");
	}
      } else {
	if (data != state.curside) {
	  trs_disk_unimpl(state.currcommand, "false head number");
	}
      }
      state.format = FMT_SECID;
      break;
    case FMT_SECID:
      if (d->emutype == REAL) {
	d->u.real.buf[d->u.real.fmt_nbytes++] = data;
      } else if (d->emutype == JV1) {
	if (data >= JV1_SECPERTRK) {
	  trs_disk_unimpl(state.currcommand, "JV1 sector number >= 10");
	}
      } else {
	state.format_sec = data;
      }
      state.format = FMT_SIZEID;
      break;
    case FMT_SIZEID:
      if (data > 0x03) {
	trs_disk_unimpl(state.currcommand, "invalid sector size");
      }
      if (d->emutype == JV3) {
	int id_index;
	id_index = jv3_alloc_sector(d, data);
	if (id_index == -1) {
	  /* Data structure full */
	  state.status |= TRSDISK_WRITEFLT;
	  state.bytecount = 0;
	  state.format_bytecount = 0;
	  state.format = FMT_DONE;
	  break;
	}
	d->u.jv3.sorted_valid = 0;
	d->u.jv3.id[id_index].track = d->phytrack;
	d->u.jv3.id[id_index].sector = state.format_sec;
	d->u.jv3.id[id_index].flags =
	  (state.curside ? JV3_SIDE : 0) | (state.density ? JV3_DENSITY : 0) |
	  ((data & 3) ^ 1);
	state.format_sec = id_index;

      } else if (d->emutype == REAL) {
	d->u.real.buf[d->u.real.fmt_nbytes++] = data;
	if (d->u.real.size_code != -1 && d->u.real.size_code != data) {
	  trs_disk_unimpl(state.currcommand,
			  "varying sector size on same track on real floppy");
	}
	d->u.real.size_code = data;
      } else {
	if (data != 0x01) {
	  trs_disk_unimpl(state.currcommand, "sector size != 256");
	}
      }
      state.format = FMT_GAP2;
      break;
    case FMT_GAP2:
      if ((data & 0xfc) == 0xf8) {
	/* Found a DAM */
        if (d->emutype == REAL) {
	  switch (data) {
	  case 0xfb:  /* Standard DAM */
	    break;
	  case 0xfa:
	    if (state.density) {
	      /* This DAM is illegal, but SuperUtility uses it, so
		 ignore the error.  This seems to be a bug in SU for
		 Model I, where it meant to use F8 instead.  I think
		 the WD controller would read back the FA as FB, so we
		 treat it as FB here.  */
	    } else {
	      if (trs_disk_truedam) {
		error("format DAM FA on real floppy");
	      }
	    }
	    break;
	  case 0xf9:
	    if (trs_disk_truedam) {
	      error("format DAM F9 on real floppy");
	    }
	    break;
	  case 0xf8:
	    /* This is probably needed by Model III TRSDOS, but it is
	       a pain to implement.  We would have to remember to do a
	       Write Deleted after the format is complete to change
	       the DAM.
	    */
	    error("format DAM F8 on real floppy");
	    break;
	  }
	} else if (d->emutype == JV1) {
          switch (data) {
	  case 0xf9:
	    trs_disk_unimpl(state.currcommand, "JV1 DAM cannot be F9");
	    break;
	  case 0xf8:
	  case 0xfa:
	    if (d->phytrack != 17)
	      trs_disk_unimpl(state.currcommand,
			      "JV1 directory track must be 17");
	    break;
	  default: /* impossible */
	  case 0xfb:
	    break;
	  }
	} else /* JV3 */ {
	  if (state.density) {
	    /* Double density */
	    switch (data) {
	    case 0xf8: /* Standard deleted DAM */
	    case 0xf9: /* Illegal, probably never used; ignore error. */
	      d->u.jv3.id[state.format_sec].flags |= JV3_DAMDDF8;
	      break;
	    case 0xfb: /* Standard DAM */
	    case 0xfa: /* Illegal, but SuperUtility uses it! */
	    default:   /* Impossible */
	      d->u.jv3.id[state.format_sec].flags |= JV3_DAMDDFB;
	      break;
	    }
	  } else {
	    /* Single density */
	    switch (data) {
	    case 0xf8:
	      if (trs_disk_truedam) {
		d->u.jv3.id[state.format_sec].flags |= JV3_DAMSDF8;
	      } else {
		d->u.jv3.id[state.format_sec].flags |= JV3_DAMSDFA;
	      }
	      break;
	    case 0xf9:
	      d->u.jv3.id[state.format_sec].flags |= JV3_DAMSDF9;
	      break;
	    case 0xfa:
	      d->u.jv3.id[state.format_sec].flags |= JV3_DAMSDFA;
	      break;
	    default: /* impossible */
	    case 0xfb:
	      d->u.jv3.id[state.format_sec].flags |= JV3_DAMDDFB;
	      break;
	    }
	  }
	}
	if (d->emutype == JV3) {
	  /* Prepare to write the data */
	  fseek(d->file, offset(d, state.format_sec), 0);
	  state.format_bytecount = id_index_to_size(d, state.format_sec);
	} else if (d->emutype == JV1) {
	  state.format_bytecount = JV1_SECSIZE;
	} else if (d->emutype == REAL) {
	  state.format_bytecount = size_code_to_size(d->u.real.size_code);
	}
	state.format_gap[2] = state.format_gapcnt;
	state.format_gapcnt = 0;
	state.format = FMT_DATA;
      } else if (data == 0xfe) {
	/* False ID: there was no DAM for following data */
	if (d->emutype != JV3) {
	  trs_disk_unimpl(state.currcommand, "false sector ID (no data)");
	} else {
	  /* We do not have a flag for this; try using CRC error */
	  warn("recording false sector ID as CRC error");
	  d->u.jv3.id[state.format_sec].flags |= JV3_ERROR;

	  /* Write the sector id */
	  fseek(d->file, idoffset(d, state.format_sec), 0);
	  c = fwrite(&d->u.jv3.id[state.format_sec], sizeof(SectorId), 1, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	}
	goto got_idam2;
      } else {
	state.format_gapcnt++;
      }
      break;
    case FMT_DATA:
      if (data == 0xfe) {
	/* Short sector with intentional CRC error */
	if (d->emutype == JV3) {
	  d->u.jv3.id[state.format_sec].flags |= JV3_NONIBM | JV3_ERROR;
	  if (trs_disk_debug_flags & DISKDEBUG_VTOS3) {
	    debug("non-IBM sector: drv 0x%02x, sid %d,"
		  " trk 0x%02x, sec 0x%02x\n",
		  state.curdrive, state.curside,
		  d->u.jv3.id[state.format_sec].track,
		  d->u.jv3.id[state.format_sec].sector);
	  }
	  /* Write the sector id */
	  fseek(d->file, idoffset(d, state.format_sec), 0);
	  c = fwrite(&d->u.jv3.id[state.format_sec],
		     sizeof(SectorId), 1, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  goto got_idam;
	} else {
	  trs_disk_unimpl(state.currcommand, "JV1 non-IBM sector");
	}
      }
      if (d->emutype == JV3) {
	c = putc(data, d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      } else if (d->emutype == REAL) {
	d->u.real.fmt_fill = data;
      }
      if (--state.format_bytecount <= 0) {
	state.format = FMT_DCRC;
      }
      break;
    case FMT_DCRC:
      if (data == 0xf7) {
	state.bytecount--;  /* two bytes are written */
      } else {
	/* Intentional CRC error */
	if (d->emutype != JV3) {
	  trs_disk_unimpl(state.currcommand, "intentional CRC error");
	} else {
	  d->u.jv3.id[state.format_sec].flags |= JV3_ERROR;
	}
      }
      if (d->emutype == JV3) {
	/* Write the sector id */
	fseek(d->file, idoffset(d, state.format_sec), 0);
	c = fwrite(&d->u.jv3.id[state.format_sec], sizeof(SectorId), 1, d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
      }
      state.format = FMT_GAP3;
      break;
    case FMT_DONE:
      break;
    case FMT_GAP4:
    case FMT_IAM:
    case FMT_IDAM:
    case FMT_IDCRC:
    case FMT_DAM:
    default:
      error("error in format state machine");
      break;
    }
  default:
    break;
  }
  state.data = data;
  return;
}

Uint8
trs_disk_status_read(void)
{
  static int last_status = -1;

  if (trs_disk_nocontroller) return 0xff;
  type1_status();
  if (!(state.status & TRSDISK_NOTRDY)) {
    if (state.motor_timeout - z80_state.t_count > TSTATE_T_MID) {
      /* Subtraction wrapped; motor stopped */
      state.status |= TRSDISK_NOTRDY;
    }
  }
  if ((trs_disk_debug_flags & DISKDEBUG_FDCREG) &&
      state.status != last_status) {
    debug("status_read() => 0x%02x pc 0x%04x\n", state.status, Z80_PC);
    last_status = state.status;
  }

#if BOGUS
  /* Clear intrq unless user did a Force Interrupt with immediate interrupt. */
  /* The 17xx data sheets say this is how it is supposed to work, but it
   * makes Model I SuperUtility hang due to the interrupt routine failing
   * to clear the interrupt.  I suspect the data sheets are wrong.
   */
  if (!(((state.currcommand & TRSDISK_CMDMASK) == TRSDISK_FORCEINT) &&
	((state.currcommand & 0x08) != 0)))
#else
  /* Clear intrq always */
#endif
    {
      /* Don't call trs_schedule_event, which could cancel a pending
       *  interrupt that should occur later and prevent it from ever
       *  happening; just clear the interrupt right now.
       */
      trs_disk_intrq_interrupt(0);
    }

  return state.status;
}

void
trs_disk_command_write(Uint8 cmd)
{
  int id_index, non_ibm, goal_side, new_status;
  DiskState *d = &disk[state.curdrive];
  trs_event_func event;

  if (trs_show_led)
    trs_disk_led(state.curdrive, 1);

  if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
    debug("command_write(0x%02x) pc 0x%04x\n", cmd, Z80_PC);
  }

  if (eg3200) {
    if ((cmd & 0xF8) == 0xF8) {
      state.density = cmd & 1;
      return;
    }
  }
  /* Handle DMK partial track reformat */
  if (d->emutype == DMK &&
      (state.currcommand & ~TRSDISK_EBIT) == TRSDISK_WRITETRK &&
      state.format != FMT_DONE) {
    /* Interrupted format: must write out partial track */
    Uint8 oldtkhdr[DMK_TKHDR_SIZE];
    int c, i, j, idamp;

    if (trs_disk_debug_flags & DISKDEBUG_DMK) {
      debug("partial track format dens %d tk %d side %d\n",
	    state.density, d->phytrack, state.curside);
    }

    /* Fetch old IDAM pointers if any */
    fseek(d->file, DMK_HDR_SIZE +
	  (d->phytrack * d->u.dmk.nsides + state.curside) *
	  d->u.dmk.tracklen, 0);
    c = fread(oldtkhdr, DMK_TKHDR_SIZE, 1, d->file);
    if (c == 1) {
      /* Copy any pointers to IDAMs that are not being overwritten */
      i = 0;
      j = d->u.dmk.nextidam;
      while (i < DMK_TKHDR_SIZE) {
	idamp = (oldtkhdr[i] + (oldtkhdr[i + 1] << 8)) & DMK_IDAMP_BITS;
	if (idamp == 0 || idamp == DMK_IDAMP_BITS) break;
	if (idamp < d->u.dmk.curbyte) {
	  /* IDAM overwritten; don't copy */
	  i += 2;
	  if (trs_disk_debug_flags & DISKDEBUG_DMK) {
	    debug("  discarding physec %d\n", i);
	  }
	} else {
	  /* IDAM not overwritten; need to copy in */
	  if (j >= DMK_TKHDR_SIZE) {
	    /* No room */
	    error("DMK reformatting adds too many sectors to track");
	    break;
	  }
	  d->u.dmk.buf[j++] = oldtkhdr[i++];
	  d->u.dmk.buf[j++] = oldtkhdr[i++];
	  if (trs_disk_debug_flags & DISKDEBUG_DMK) {
	    debug("  preserving physec %d as %d\n", i, j);
	  }
	}
      }
    } else {
      if (trs_disk_debug_flags & DISKDEBUG_FDCREG) {
	debug("  no existing sectors\n");
      }
    }
    /* Write modified portion of track only */
    fseek(d->file, DMK_HDR_SIZE +
	  (d->phytrack * d->u.dmk.nsides + state.curside) *
	  d->u.dmk.tracklen, 0);
    fwrite(d->u.dmk.buf, d->u.dmk.curbyte, 1, d->file);
    if (d->phytrack >= d->u.dmk.ntracks) {
      d->u.dmk.ntracks = d->phytrack + 1;
      fseek(d->file, DMK_NTRACKS, 0);
      putc(d->u.dmk.ntracks, d->file);
    }
    fflush(d->file);

    /* Invalidate buffer since not all data is here */
    d->u.dmk.curtrack = d->u.dmk.curside = -1;
    state.format = FMT_DONE;
  }

  /* Cancel any ongoing command */
  event = trs_event_scheduled();
  if (event == trs_disk_lostdata || event == trs_disk_intrq_interrupt) {
    trs_cancel_event();
  }
  trs_disk_intrq_interrupt(0);
  state.bytecount = 0;
  state.currcommand = cmd;
  switch (cmd & TRSDISK_CMDMASK) {

  case TRSDISK_RESTORE:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("restore 0x%02x drv %d\n", cmd, state.curdrive);
    }
    state.last_readadr = -1;
    d->phytrack = 0;
    state.track = 0;
    state.status = TRSDISK_TRKZERO|TRSDISK_BUSY;
    if (d->emutype == REAL) real_restore(state.curdrive);
    /* Should this set lastdirection? */
    if (cmd & TRSDISK_VBIT) verify();
    trs_schedule_event(trs_disk_done, 0, 2000);
    break;

  case TRSDISK_SEEK:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("seek 0x%02x drv %d ptk %d otk %d ntk %d\n",
	    cmd, state.curdrive, d->phytrack, state.track, state.data);
    }
    state.last_readadr = -1;
    d->phytrack += (state.data - state.track);
    state.track = state.data;
    if (d->phytrack <= 0) {
      d->phytrack = 0;		/* state.track too? */
      state.status = TRSDISK_TRKZERO|TRSDISK_BUSY;
    } else {
      state.status = TRSDISK_BUSY;
    }
    if (d->emutype == REAL) real_seek();
    /* Should this set lastdirection? */
    if (cmd & TRSDISK_VBIT) verify();
    trs_schedule_event(trs_disk_done, 0, 2000);
    break;

  case TRSDISK_STEP:
  case TRSDISK_STEPU:
  step:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("step%s %s 0x%02x drv %d ptk %d otk %d\n",
	    (cmd & TRSDISK_UBIT) ? "u" : "",
	    (state.lastdirection < 0) ? "out" : "in", cmd,
	    state.curdrive, d->phytrack, state.track);
    }
    state.last_readadr = -1;
    d->phytrack += state.lastdirection;
    if (cmd & TRSDISK_UBIT) {
      state.track += state.lastdirection;
    }
    if (d->phytrack <= 0) {
      d->phytrack = 0;		/* state.track too? */
      state.status = TRSDISK_TRKZERO|TRSDISK_BUSY;
    } else {
      state.status = TRSDISK_BUSY;
    }
    if (d->emutype == REAL) real_seek();
    if (cmd & TRSDISK_VBIT) verify();
    trs_schedule_event(trs_disk_done, 0, 2000);
    break;

  case TRSDISK_STEPIN:
  case TRSDISK_STEPINU:
    state.lastdirection = 1;
    goto step;

  case TRSDISK_STEPOUT:
  case TRSDISK_STEPOUTU:
    state.lastdirection = -1;
    goto step;

  case TRSDISK_READ:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("read 0x%02x drv %d sid %d ptk %d tk %d sec %d %sden\n", cmd,
	    state.curdrive, state.curside, d->phytrack,
	    state.track, state.sector, state.density ? "d" : "s");
    }
    state.last_readadr = -1;
    state.status = 0;
    non_ibm = 0;
    goal_side = -1;
    new_status = 0;
    if (state.controller == TRSDISK_P1771) {
      if (!(cmd & TRSDISK_BBIT)) {
	if (trs_disk_debug_flags & DISKDEBUG_VTOS3) {
	  debug("non-IBM read: drv 0x%02x, sid %d, trk 0x%02x, sec 0x%02x\n",
		state.curdrive, state.curside, state.track, state.sector);
	}
	if (d->emutype == REAL) {
	  trs_disk_unimpl(cmd, "non-IBM read on real floppy");
	}
	non_ibm = 1;
      } else {
	if (trs_disk_debug_flags & DISKDEBUG_VTOS3) {
	  if (state.sector >= 0x7c) {
	    debug("IBM read: drv 0x%02x, sid %d, trk 0x%02x, sec 0x%02x\n",
		  state.curdrive, state.curside, state.track, state.sector);
	  }
	}
      }
    } else {
      if (cmd & TRSDISK_CBIT) {
	goal_side = (cmd & TRSDISK_BBIT) != 0;
      }
    }
    if (d->emutype == REAL) {
      real_read();
      break;
    }
    id_index = search(state.sector, goal_side);
    if (id_index == -1) {
      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_done, 0, 512);
    } else {
      if (d->emutype == JV1) {

	if (d->phytrack == 17) {
	  if (state.controller == TRSDISK_P1771) {
	    new_status = TRSDISK_1771_FA;
	  } else {
	    new_status = TRSDISK_1791_F8;
	  }
	}
	state.bytecount = JV1_SECSIZE;
	fseek(d->file, offset(d, id_index), 0);

      } else if (d->emutype == JV3) {

	if (state.controller == TRSDISK_P1771) {
	  switch (d->u.jv3.id[id_index].flags & JV3_DAM) {
	  case JV3_DAMSDFB:
	    new_status = TRSDISK_1771_FB;
	    break;
	  case JV3_DAMSDFA:
	    new_status = TRSDISK_1771_FA;
	    break;
	  case JV3_DAMSDF9:
	    new_status = TRSDISK_1771_F9;
	    break;
	  case JV3_DAMSDF8:
	    new_status = TRSDISK_1771_F8;
	    break;
	  }
	} else if (state.density == 0) {
	  /* single density 179x */
	  switch (d->u.jv3.id[id_index].flags & JV3_DAM) {
	  case JV3_DAMSDFB:
	    new_status = TRSDISK_1791_FB;
	    break;
	  case JV3_DAMSDFA:
	    if (trs_disk_truedam) {
	      new_status = TRSDISK_1791_FB;
	    } else {
	      new_status = TRSDISK_1791_F8;
	    }
	    break;
	  case JV3_DAMSDF9:
	    new_status = TRSDISK_1791_F8;
	    break;
	  case JV3_DAMSDF8:
	    new_status = TRSDISK_1791_F8;
	    break;
	  }
	} else {
	  /* double density 179x */
	  switch (d->u.jv3.id[id_index].flags & JV3_DAM) {
	  default: /*impossible*/
	  case JV3_DAMDDFB:
	    new_status = TRSDISK_1791_FB;
	    break;
	  case JV3_DAMDDF8:
	    new_status = TRSDISK_1791_F8;
	    break;
	  }
	}
	if (d->u.jv3.id[id_index].flags & JV3_ERROR) {
	  new_status |= TRSDISK_CRCERR;
	}
	if (non_ibm) {
	  state.bytecount = 16;
	} else {
	  state.bytecount = id_index_to_size(d, id_index);
	}
	fseek(d->file, offset(d, id_index), 0);

      } else /* d->emutype == DMK */ {

	/* max distance past ID CRC to search for DAM */
	int damlimit = state.density ? 43 : 30; /* ref 1791 datasheet */
	Uint8 dam = 0;

	/* DMK search dumps the size code into state.bytecount; adjust
           to real bytecount here */
	if (non_ibm) {
	  state.bytecount = 16 * (((state.bytecount - 1)&0xff) + 1);
	} else {
	  state.bytecount = 128 << (state.bytecount & 3);
	}

	/* search for valid DAM */
	while (--damlimit >= 0) {
	  dam = d->u.dmk.buf[id_index];
	  id_index += dmk_incr(d);
	  if (0xf8 <= dam && dam <= 0xfb) {
	    /* got one! */
	    break;
	  }
	}
	if (damlimit < 0) {
	  /* found ID with good CRC but no following DAM; fail */
	  state.status |= TRSDISK_BUSY;
	  trs_schedule_event(trs_disk_done, TRSDISK_NOTFOUND, 512);
	  break;
	}

	/* Set flags for DAM */
	if (state.controller == TRSDISK_P1771) {
	  /* 1771 */
	  switch (dam) {
	  case 0xfb:
	    new_status = TRSDISK_1771_FB;
	    break;
	  case 0xfa:
	    new_status = TRSDISK_1771_FA;
	    break;
	  case 0xf9:
	    new_status = TRSDISK_1771_F9;
	    break;
	  case 0xf8:
	    new_status = TRSDISK_1771_F8;
	    break;
	  }
	} else /* state.controller == TRSDISK_P1791 */ {
	  switch (dam) {
	  case 0xfb:
	    new_status = TRSDISK_1791_FB;
	    break;
	  case 0xfa:
	    /* Note: Illegal in DDEN but Write Track can still
	       generate it, and of course 1771 can generate in SDEN */
	    if (trs_disk_truedam) {
	      new_status = TRSDISK_1791_FB;
	    } else {
	      new_status = TRSDISK_1791_F8;
	    }
	    break;
	  case 0xf9:
	    /* Note: Illegal in DDEN but Write Track can still
	       generate it, and of course 1771 can generate in SDEN */
	    new_status = TRSDISK_1791_F8;
	    break;
	  case 0xf8:
	    new_status = TRSDISK_1791_F8;
	    break;
	  }
	}
	state.crc = calc_crc((state.density
			      ? 0xcdb4 /* CRC of a1 a1 a1 */
			      : 0xffff), dam);

	d->u.dmk.curbyte = id_index;

      } /* end if (d->emutype == ...) */

      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_firstdrq, new_status, 64);
    }
    break;

  case TRSDISK_READM:
    state.last_readadr = -1;
    trs_disk_unimpl(cmd, "read multiple");
    break;

  case TRSDISK_WRITE:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("write 0x%02x drv %d sid %d ptk %d tk %d sec %d %sden\n",
	    cmd, state.curdrive, state.curside, d->phytrack,
	    state.track, state.sector, state.density ? "d" : "s");
    }
    state.last_readadr = -1;
    state.status = 0;
    non_ibm = 0;
    goal_side = -1;
    if (state.controller == TRSDISK_P1771) {
      if (!(cmd & TRSDISK_BBIT)) {
	if (trs_disk_debug_flags & DISKDEBUG_VTOS3) {
	  debug("non-IBM write drv 0x%02x, sid %d, trk 0x%02x, sec 0x%02x\n",
		state.curdrive, state.curside, state.track, state.sector);
	}
	if (d->emutype == REAL) {
	  trs_disk_unimpl(cmd, "non-IBM write on real floppy");
	}
	non_ibm = 1;
      } else {
	if (trs_disk_debug_flags & DISKDEBUG_VTOS3) {
	  if (state.sector >= 0x7c) {
	    debug("IBM write: drv 0x%02x, sid %d, trk 0x%02x, sec 0x%02x\n",
		  state.curdrive, state.curside, state.track, state.sector);
	  }
	}
      }
    } else {
      if (cmd & TRSDISK_CBIT) {
	goal_side = (cmd & TRSDISK_BBIT) != 0;
      }
    }
    if (d->emutype == REAL) {
      state.status = TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      trs_schedule_event(trs_disk_lostdata, state.currcommand,
			 500000 * z80_state.clockMHz);
      state.bytecount = size_code_to_size(d->u.real.size_code);
      break;
    }
    if (d->writeprot) {
      state.status = TRSDISK_WRITEPRT;
      break;
    }
    id_index = search(state.sector, goal_side);
    if (id_index == -1) {
      state.status |= TRSDISK_BUSY;
      trs_schedule_event(trs_disk_done, 0, 512);
    } else {
      int jv3dam = 0, dam = 0;
      if (state.controller == TRSDISK_P1771) {
	switch (cmd & (TRSDISK_CBIT|TRSDISK_DBIT)) {
	case 0:
	  dam = 0xfb;
	  jv3dam = JV3_DAMSDFB;
	  break;
	case 1:
	  dam = 0xfa;
	  jv3dam = JV3_DAMSDFA;
	  break;
	case 2:
	  dam = 0xf9;
	  jv3dam = JV3_DAMSDF9;
	  break;
	case 3:
	  if (trs_disk_truedam) {
	    dam = 0xf8;
	    jv3dam = JV3_DAMSDF8;
	  } else {
	    dam = 0xfa;
	    jv3dam = JV3_DAMSDFA;
	  }
	  break;
	}
      } else if (state.density == 0) {
	/* 179x single */
	switch (cmd & TRSDISK_DBIT) {
	case 0:
	  dam = 0xfb;
	  jv3dam = JV3_DAMSDFB;
	  break;
	case 1:
	  if (trs_disk_truedam) {
	    dam = 0xf8;
	    jv3dam = JV3_DAMSDF8;
	  } else {
	    dam = 0xfa;
	    jv3dam = JV3_DAMSDFA;
	  }
	  break;
	}
      } else {
	/* 179x double */
	switch (cmd & TRSDISK_DBIT) {
	case 0:
	  dam = 0xfb;
	  jv3dam = JV3_DAMDDFB;
	  break;
	case 1:
	  dam = 0xf8;
	  jv3dam = JV3_DAMDDF8;
	  break;
	}
      }

      if (d->emutype == JV1) {
	if (dam == 0xf9) {
	  trs_disk_unimpl(state.currcommand, "JV1 DAM cannot be F9");
	} else if ((dam == 0xfb) == (d->phytrack == 17)) {
	  trs_disk_unimpl(state.currcommand, "JV1 directory must be track 17");
	  break;
	}
	state.bytecount = JV1_SECSIZE;
	fseek(d->file, offset(d, id_index), 0);

      } else if (d->emutype == JV3) {
	SectorId *sid = &d->u.jv3.id[id_index];
	Uint8 newflags = sid->flags;
	newflags &= ~(JV3_ERROR|JV3_DAM); /* clear CRC error and DAM */
	newflags |= jv3dam;
	if (newflags != sid->flags) {
	  int c;
	  fseek(d->file, idoffset(d, id_index)
		         + ((char *) &sid->flags) - ((char *) sid), 0);
	  c = putc(newflags, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  c = fflush(d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  sid->flags = newflags;
	}

	/* Kludge for VTOS 3.0 */
	if (sid->flags & JV3_NONIBM) {
	  int i, j, c;
	  /* Smash following sectors. This is especially a kludge because
	     it uses the sector numbers, not the known physical sector
	     order. */
	  for (i = state.sector + 1; i <= 0x7f; i++) {
	    j = search(i, -1);
	    if (j != -1) {
	      if (trs_disk_debug_flags & DISKDEBUG_VTOS3) {
		debug("smashing tk %d sector 0x%02x id_index %d\n",
		      state.track, i, j);
	      }
	      jv3_free_sector(d, j);
	      c = fflush(d->file);
	      if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	    }
	    /* Smash only one for non-IBM write */
	    if (non_ibm) break;
	  }
	}
	/* end kludge */

	if (non_ibm) {
	  state.bytecount = 16;
	} else {
	  state.bytecount = id_index_to_size(d, id_index);
	}
	fseek(d->file, offset(d, id_index), 0);

      } else /* d->emutype == DMK */ {
	int c, nzeros, i;

	/* DMK search dumps the size code into state.bytecount; adjust
           to real bytecount here */
	if (non_ibm) {
	  state.bytecount = 16 * (((state.bytecount - 1)&0xff) + 1);
	} else {
	  state.bytecount = 128 << (state.bytecount & 3);
	}

	/* Skip initial part of gap, per 1771 and 179x data sheets */
	id_index += 11 * (state.density ? 2 : 1) * dmk_incr(d);
	fseek(d->file, (DMK_HDR_SIZE +
			(d->u.dmk.curtrack*d->u.dmk.nsides + d->u.dmk.curside)
			* d->u.dmk.tracklen + id_index), 0);

	/* Write remaining gap (per data sheets) and DAM */
	nzeros = 6 * (state.density ? 2 : 1) * dmk_incr(d);
	for (i = 0; i < nzeros; i++) {
	  c = putc(0, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  d->u.dmk.buf[id_index++] = 0;
	}
	if (state.density) {
	  for (i = 0; i < 3; i++) {
	    c = putc(0xa1, d->file);
	    if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	    d->u.dmk.buf[id_index++] = 0xa1;
	  }
	}
	c = putc(dam, d->file);
	if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	d->u.dmk.buf[id_index++] = dam;
	if (dmk_incr(d) == 2) {
	  c = putc(dam, d->file);
	  if (c == EOF) state.status |= TRSDISK_WRITEFLT;
	  d->u.dmk.buf[id_index++] = dam;
	}

	/* Initialize CRC */
	state.crc = calc_crc((state.density
			      ? 0xcdb4 /* CRC of a1 a1 a1 */
			      : 0xffff), dam);

	d->u.dmk.curbyte = id_index;

      } /* end if (d->emutype == ...) */

      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      trs_schedule_event(trs_disk_lostdata, state.currcommand,
			 500000 * z80_state.clockMHz);
    }
    break;

  case TRSDISK_WRITEM:
    state.last_readadr = -1;
    if (d->writeprot) {
      state.status = TRSDISK_WRITEPRT;
      break;
    }
    trs_disk_unimpl(cmd, "write multiple");
    break;

  case TRSDISK_READADR:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("readadr 0x%02x drv %d sid %d ptk %d tk %d last %d %sden\n",
	    cmd, state.curdrive, state.curside, d->phytrack, state.track,
	    state.last_readadr, state.density ? "d" : "s");
    }
    state.data = 0; /* workaround for apparent SU1 bug */
    if (state.density) {
      state.crc = 0xb230;  /* CRC of a1 a1 a1 fe */
    } else {
      state.crc = 0xef21;  /* CRC of fe */
    }
    if (d->emutype == REAL) {
      real_readadr();
      break;
    } else if (d->emutype == JV1 || d->emutype == JV3) {
      int totbyt, i, ts, denok;
      float a, b, bytlen;
      id_index = search_adr();
      if (id_index == -1) {
	state.status = TRSDISK_BUSY;
	state.bytecount = 0;
	trs_schedule_event(trs_disk_done, TRSDISK_NOTFOUND,
			   1000000*z80_state.clockMHz);
	break;
      }
      /* Compute how long it should have taken for this sector to come
	 by and delay by an appropriate number of t-states.  This
	 makes the "A" command in HyperZap work (on emulated floppies
	 only).  It is not terribly useful, since other important HyperZap
	 functions (like mixed-density formatting) do not work, while
	 SuperUtility and Trakcess both work fine without the delay feature.
	 Note: it would probably be better to assume the sectors are
	 positioned using nominal gap sizes (say, the ones that HyperZap
	 uses when generating tracks using the D/G subcommand) instead
	 of the even spacing nonsense below.
      */
      if (d->emutype == JV1) {
	/* Which sector header is next?  Use a rough assumption
	   that the sectors are all the same angular length (bytlen).
	*/
	a = angle();
	bytlen = (1.0 - GAP1ANGLE - GAP4ANGLE) / ((float)JV1_SECPERTRK);
	i = (int)( (a - GAP1ANGLE) / bytlen + 1.0 );
	if (i >= JV1_SECPERTRK) {
	  /* Wrap around to start of track */
	  i = 0;
	}
	b = ((float)i) * bytlen + GAP1ANGLE;
	if (b < a) b += 1.0;
	i += id_index;
      } else {
	/* Count data bytes on track.  Also check if there
	   are any sectors of the correct density. */
	i = id_index;
	totbyt = 0;
	denok = 0;
	for (;;) {
	  SectorId *sid = &d->u.jv3.id[d->u.jv3.sorted_id[i]];
	  int dden = (sid->flags & JV3_DENSITY) != 0;
	  if (sid->track != d->phytrack ||
	      (sid->flags & JV3_SIDE ? 1 : 0) != state.curside) break;
	  totbyt += (dden ? 1 : 2) *
	    id_index_to_size(d, d->u.jv3.sorted_id[i]);
	  if (dden == state.density) denok = 1;
	  i++;
	}
	if (!denok) {
	  /* No sectors of the correct density */
	  state.status = TRSDISK_BUSY;
	  state.bytecount = 0;
	  trs_schedule_event(trs_disk_done, TRSDISK_NOTFOUND,
			     1000000*z80_state.clockMHz);
	  break;
	}
	/* Which sector header is next?  Use a rough assumption that
           sectors are evenly spaced, taking up room proportional to
           their data length (and twice as much for single density).
	   Here bytlen = angular size per byte.
	*/
	a = angle();
	b = GAP1ANGLE;
	bytlen = (1.0 - GAP1ANGLE - GAP4ANGLE) / ((float)totbyt);
	i = id_index;
	for (;;) {
	  SectorId *sid = &d->u.jv3.id[d->u.jv3.sorted_id[i]];
	  if (sid->track != d->phytrack ||
	      (sid->flags & JV3_SIDE ? 1 : 0) != state.curside) {
	    /* Wrap around to start of track */
	    i = id_index;
	    b = 1 + GAP1ANGLE;
	    break;
	  }
	  if (b > a && (((sid->flags & JV3_DENSITY) != 0) == state.density)) {
	    break;
	  }
	  b += ((sid->flags & JV3_DENSITY) ? 1 : 2) *
  	    id_index_to_size(d, d->u.jv3.sorted_id[i]) * bytlen;
	  i++;
	}
      }
      /* Convert angular delay to t-states */
      ts = (d->inches == 5 ? 200000 : 166667) * (b - a) * z80_state.clockMHz;

      state.status = TRSDISK_BUSY;
      state.last_readadr = i;
      state.bytecount = 6;
      trs_schedule_event(trs_disk_firstdrq, 0, ts);
      if (trs_disk_debug_flags & DISKDEBUG_READADR) {
	debug("readadr phytrack %d angle %f i %d ts %d\n",
	      d->phytrack, a, i, ts);
      }
    } else /* d->emutype == DMK */ {
      /* Compute how far it will be to the next ID in the correct density */
      float a = angle();
      int ia = a * (d->inches ? TRKSIZE_DD : TRKSIZE_8DD);
      int ib = 0;
      int i, j, idamp, dden, prev_idamp, prev_dden, ts;

      dmk_get_track(d);

      for (j = 0; j < 2; j++) {
	idamp = d->u.dmk.buf[0] + (d->u.dmk.buf[1] << 8);
	dden = (idamp & DMK_DDEN_FLAG) != 0;
	idamp = DMK_TKHDR_SIZE;

	for (i = 0; i < DMK_TKHDR_SIZE; i+=2) {
	  prev_idamp = idamp;
	  prev_dden = dden;
	  idamp = d->u.dmk.buf[i] + (d->u.dmk.buf[i + 1] << 8);
	  if (idamp == 0) break;
	  dden = (idamp & DMK_DDEN_FLAG) != 0;
	  idamp &= DMK_IDAMP_BITS;
	  if (idamp >= DMK_TRACKLEN_MAX) break;
	  ib += (idamp - prev_idamp) *
	    ((!prev_dden && (d->u.dmk.sden || d->u.dmk.ignden)) ? 2 : 1);
	  if (ib > ia && dden == state.density &&
	      d->u.dmk.buf[idamp] == 0xfe) goto found;
	}
	/* Next ID (if any) is past the index hole */
	ib = (d->inches ? TRKSIZE_DD : TRKSIZE_8DD);
      }
      /* no suitable ID found */
      state.status = TRSDISK_BUSY;
      state.bytecount = 0;
      trs_schedule_event(trs_disk_done, TRSDISK_NOTFOUND,
			 1000000*z80_state.clockMHz);
      break;
    found:
      /* Convert dden byte count to t-states */
      ts = ((float) ((ib - ia) * (d->inches == 5 ? 32 : 16)))
	* z80_state.clockMHz;

      state.status = TRSDISK_BUSY;
      state.last_readadr = i;
      state.bytecount = 6;
      state.crc = calc_crc((state.density
			    ? 0xcdb4 /* CRC of a1 a1 a1 */
			    : 0xffff),
			    d->u.dmk.buf[idamp]);
      d->u.dmk.curbyte = idamp + dmk_incr(d);
      trs_schedule_event(trs_disk_firstdrq, 0, ts);
      if (trs_disk_debug_flags & DISKDEBUG_READADR) {
	debug("readadr phytrack %d angle %f i %d ts %d\n",
	      d->phytrack, a, i, ts);
      }
    }
    break;

  case TRSDISK_READTRK:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("readtrk 0x%02x drv %d sid %d ptk %d tk %d %sden\n",
	    cmd, state.curdrive, state.curside, d->phytrack, state.track,
	    state.density ? "d" : "s");
    }
    state.last_readadr = -1;
    if (d->file == NULL) {
      /* Data sheet says we wait forever for an index pulse, ugh */
      state.status = TRSDISK_BUSY;
      state.bytecount = 0;
      break;
    }
    if (d->emutype == REAL) {
      real_readtrk();
      break;
    }
    if (d->emutype != DMK) {
      trs_disk_unimpl(cmd, "read track");
      break;
    }
    dmk_get_track(d);
    d->u.dmk.curbyte = DMK_TKHDR_SIZE;
    if (disk[state.curdrive].inches == 5) {
      state.bytecount = TRKSIZE_DD;  /* decrement by 2's if SD */
    } else {
      state.bytecount = TRKSIZE_8DD; /* decrement by 2's if SD */
    }
    state.status = TRSDISK_BUSY|TRSDISK_DRQ;
    trs_disk_drq_interrupt(1);
    trs_schedule_event(trs_disk_lostdata, state.currcommand,
		       500000 * z80_state.clockMHz);
    break;

  case TRSDISK_WRITETRK:
    state.last_readadr = -1;
    /* Really a write track? */
    if (trs_model == 1 && (cmd == TRSDISK_P1771 || cmd == TRSDISK_P1791)) {
      /* No; emulate Percom Doubler */
      state.currcommand = TRSDISK_FORCEINT;
      if (trs_disk_doubler & TRSDISK_PERCOM) {
	trs_disk_set_controller(cmd);
	/* The Doubler's 1791 is hardwired to double density */
	state.density = (state.controller == TRSDISK_P1791);
      }
    } else {
      if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
	debug("writetrk 0x%02x drv %d sid %d ptk %d tk %d %sden\n",
	      cmd, state.curdrive, state.curside, d->phytrack, state.track,
	      state.density ? "d" : "s");
      }
      /* Yes; a real write track */
      if (d->emutype != REAL && d->writeprot) {
	state.status = TRSDISK_WRITEPRT;
	break;
      }
      state.status = 0;
      if (d->file == NULL) {
	/* Data sheet says we wait forever for an index pulse, ugh */
	state.status = TRSDISK_BUSY;
	state.bytecount = 0;
	break;
      }
      if (d->emutype == JV3) {
	/* Erase track if already formatted */
	int i;
	for (i = 0; i <= d->u.jv3.last_used_id; i++) {
	  if (d->u.jv3.id[i].track == d->phytrack &&
	      ((d->u.jv3.id[i].flags & JV3_SIDE) != 0) == state.curside) {
	    jv3_free_sector(d, i);
	  }
	}
      } else if (d->emutype == REAL) {
	d->u.real.size_code = -1; /* watch for first, then check others match*/
	d->u.real.fmt_nbytes = 0; /* size of PC formatting command buffer */
      } else if (d->emutype == DMK) {
	if (state.density && d->u.dmk.sden) {
	  error("DMK disk created as single density only");
	  state.status |= TRSDISK_WRITEFLT;
	}
	if (state.curside && d->u.dmk.nsides == 1) {
	  error("DMK disk created as single sided only");
	  state.status |= TRSDISK_WRITEFLT;
	}
	d->u.dmk.curtrack = d->phytrack;
	d->u.dmk.curside = state.curside;
	memset(d->u.dmk.buf, 0, sizeof(d->u.dmk.buf));
	d->u.dmk.curbyte = DMK_TKHDR_SIZE;
	d->u.dmk.nextidam = 0;
      }
      state.status |= TRSDISK_BUSY|TRSDISK_DRQ;
      trs_disk_drq_interrupt(1);
      trs_schedule_event(trs_disk_lostdata, state.currcommand,
			 500000 * z80_state.clockMHz);
      state.format = FMT_GAP0;
      state.format_gapcnt = 0;
      if (disk[state.curdrive].inches == 5) {
	state.bytecount = TRKSIZE_DD;  /* decrement by 2's if SD */
      } else {
	state.bytecount = TRKSIZE_8DD; /* decrement by 2's if SD */
      }
    }
    break;

  case TRSDISK_FORCEINT:
    if (trs_disk_debug_flags & DISKDEBUG_FDCCMD) {
      debug("forceint 0x%02x\n", cmd);
    }
    /* Stop whatever is going on and forget it */
    trs_cancel_event();
    state.status = 0;
    type1_status();
    if ((cmd & 0x07) != 0) {
      /* Conditional interrupt features not implemented. */
      trs_disk_unimpl(cmd, "force interrupt with condition");
    } else if ((cmd & 0x08) != 0) {
      /* Immediate interrupt */
      trs_disk_intrq_interrupt(1);
    } else {
      trs_disk_intrq_interrupt(0);
    }
    break;
  }
}

/* Interface to real floppy drive */
int
real_rate(DiskState *d)
{
  if (d->inches == 5) {
    if (d->u.real.rps == 5) {
      return 2;
    } else if (d->u.real.rps == 6) {
      return 1;
    }
  } else if (d->inches == 8) {
    return 0;
  }
  trs_disk_unimpl(state.currcommand, "real_rate internal error");
  return 1;
}

void
real_error(DiskState *d, unsigned int flags, char *msg)
{
  time_t now = time(NULL);
  if (now > d->u.real.empty_timeout) {
    d->u.real.empty_timeout = time(NULL) + EMPTY_TIMEOUT;
    d->u.real.empty = 1;
  }
  if (trs_disk_debug_flags & DISKDEBUG_REALERR) {
    debug("error on real_%s\n", msg);
  }
}

void
real_ok(DiskState *d)
{
  d->u.real.empty_timeout = time(NULL) + EMPTY_TIMEOUT;
  d->u.real.empty = 0;
}

int
real_check_empty(DiskState *d)
{
#if __linux
  int reset_now = 0;
  struct floppy_raw_cmd raw_cmd;
  int i = 0;

  if (time(NULL) <= d->u.real.empty_timeout) return d->u.real.empty;

  if (d->file == NULL) {
    d->u.real.empty = 1;
    return 1;
  }

  ioctl(fileno(d->file), FDRESET, &reset_now);

  /* Do a read id command.  Assume a disk is in the drive iff
     we get a nonnegative status back from the ioctl. */
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = state.density ? 0x4a : 0x0a; /* read ID */
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd_count = i;
  raw_cmd.data = NULL;
  raw_cmd.length = 0;
  if (ioctl(fileno(d->file), FDRAWCMD, &raw_cmd) < 0) {
    real_error(d, raw_cmd.flags, "check_empty");
  } else {
    real_ok(d);
  }
#else
  trs_disk_unimpl(state.currcommand, "check for empty on real floppy");
#endif
  return d->u.real.empty;
}

void
real_verify(void)
{
  /* Verify that head is on the expected track */
  /*!! ignore for now*/
}

void
real_restore(int curdrive)
{
#if __linux
  DiskState *d = &disk[curdrive];
  struct floppy_raw_cmd raw_cmd;
  int i = 0;

  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = FD_RECALIBRATE;
  raw_cmd.cmd[i++] = 0;
  raw_cmd.cmd_count = i;
  if (ioctl(fileno(d->file), FDRAWCMD, &raw_cmd) < 0) {
    real_error(d, raw_cmd.flags, "restore");
    state.status |= TRSDISK_SEEKERR;
    return;
  }
#else
  trs_disk_unimpl(state.currcommand, "restore real floppy");
#endif
}

void
real_seek(void)
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int i = 0;

  /* Always use a recal if going to track 0.  This should help us
     recover from confusion about what track the disk is really on.
     I'm still not sure why the confusion sometimes arises. */
  if (d->phytrack == 0) {
    real_restore(state.curdrive);
    return;
  }

  state.last_readadr = -1;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.length = 256;
  raw_cmd.data = NULL;
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_INTR;
  raw_cmd.cmd[i++] = FD_SEEK;
  raw_cmd.cmd[i++] = 0;
  raw_cmd.cmd[i++] = d->phytrack * d->real_step;
  raw_cmd.cmd_count = i;
  if (ioctl(fileno(d->file), FDRAWCMD, &raw_cmd) < 0) {
    real_error(d, raw_cmd.flags, "seek");
    state.status |= TRSDISK_SEEKERR;
    return;
  }
#else
  trs_disk_unimpl(state.currcommand, "seek real floppy");
#endif
}

void
real_read(void)
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int i, retry, new_status;

  /* Try once at each supported sector size */
  retry = 0;
  for (;;) {
    state.status = 0;
    new_status = 0;
    memset(&raw_cmd, 0, sizeof(raw_cmd));
    raw_cmd.rate = real_rate(d);
    raw_cmd.flags = FD_RAW_READ | FD_RAW_INTR;
    i = 0;
    raw_cmd.cmd[i++] = state.density ? 0x46 : 0x06;
    raw_cmd.cmd[i++] = state.curside ? 4 : 0;
    raw_cmd.cmd[i++] = state.track;
    raw_cmd.cmd[i++] = state.curside;
    raw_cmd.cmd[i++] = state.sector;
    raw_cmd.cmd[i++] = d->u.real.size_code;
    raw_cmd.cmd[i++] = 255;
    raw_cmd.cmd[i++] = 0x0a;
    raw_cmd.cmd[i++] = 0xff; /* unused */
    raw_cmd.cmd_count = i;
    raw_cmd.data = (void*) d->u.real.buf;
    raw_cmd.length = 128 << d->u.real.size_code;
    if (ioctl(fileno(d->file), FDRAWCMD, &raw_cmd) < 0) {
      real_error(d, raw_cmd.flags, "read");
      new_status |= TRSDISK_NOTFOUND;
    } else {
      real_ok(d); /* premature? */
      if (raw_cmd.reply[1] & 0x04) {
	/* Could have been due to wrong sector size, so we'll retry
	   internally in each other size before returning an error. */
	if (trs_disk_debug_flags & DISKDEBUG_REALSIZE) {
	  debug("real_read not fnd: side %d tk %d sec %d size 0%d phytk %d\n",
		state.curside, state.track, state.sector, d->u.real.size_code,
		d->phytrack*d->real_step);
	}
#if SIZERETRY
	d->u.real.size_code = (d->u.real.size_code + 1) % 4;
	if (++retry < 4) {
	  continue; /* retry */
	}
#endif
	new_status |= TRSDISK_NOTFOUND;
      }
      if (raw_cmd.reply[1] & 0x81) new_status |= TRSDISK_NOTFOUND;
      if (raw_cmd.reply[1] & 0x20) {
	new_status |= TRSDISK_CRCERR;
	if (!(raw_cmd.reply[2] & 0x20)) new_status |= TRSDISK_NOTFOUND;
      }
      if (raw_cmd.reply[1] & 0x10) new_status |= TRSDISK_LOSTDATA;
      if (raw_cmd.reply[2] & 0x40) {
	if (state.controller == TRSDISK_P1771) {
	  if (trs_disk_truedam) {
	    new_status |= TRSDISK_1771_F8;
	  } else {
	    new_status |= TRSDISK_1771_FA;
	  }
	} else {
	  new_status |= TRSDISK_1791_F8;
	}
      }
      if (raw_cmd.reply[2] & 0x20) new_status |= TRSDISK_CRCERR;
      if (raw_cmd.reply[2] & 0x13) new_status |= TRSDISK_NOTFOUND;
      if ((new_status & TRSDISK_NOTFOUND) == 0) {
	/* Start read */
	state.status = TRSDISK_BUSY;
	trs_schedule_event(trs_disk_firstdrq, new_status, 64);
	state.bytecount = size_code_to_size(d->u.real.size_code);
	return;
      }
    }
    break; /* exit retry loop */
  }
  /* Sector not found; fail */
  state.status = TRSDISK_BUSY;
  trs_schedule_event(trs_disk_done, new_status, 512);
#else
  trs_disk_unimpl(state.currcommand, "read real floppy");
#endif
}

void
real_write(void)
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int i = 0;

  state.status = 0;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
  if (trs_disk_truedam && !state.density) {
    switch (state.currcommand & 0x03) {
    case 0:
    case 3:
      break;
    case 1:
      error("writing FA DAM on real floppy");
      break;
    case 2:
      error("writing F9 DAM on real floppy");
      break;
    }
  }
  /* Use F8 DAM for F8, F9, or FA */
  raw_cmd.cmd[i++] = ((state.currcommand &
		       (state.controller == TRSDISK_P1771 ? 0x03 : 0x01))
		      ? 0x09 : 0x05) | (state.density ? 0x40 : 0x00);
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd[i++] = state.track;
  raw_cmd.cmd[i++] = state.curside;
  raw_cmd.cmd[i++] = state.sector;
  raw_cmd.cmd[i++] = d->u.real.size_code;
  raw_cmd.cmd[i++] = 255;
  raw_cmd.cmd[i++] = 0x0a;
  raw_cmd.cmd[i++] = 0xff; /* 256 */
  raw_cmd.cmd_count = i;
  raw_cmd.data = (void*) d->u.real.buf;
  raw_cmd.length = 128 << d->u.real.size_code;
  if (ioctl(fileno(d->file), FDRAWCMD, &raw_cmd) < 0) {
    real_error(d, raw_cmd.flags, "write");
    state.status |= TRSDISK_NOTFOUND;
  } else {
    real_ok(d); /* premature? */
    if (raw_cmd.reply[1] & 0x04) {
      state.status |= TRSDISK_NOTFOUND;
      /* Could have been due to wrong sector size.  Presumably
         the Z80 software will do some retries, so we'll cause
         it to try the next sector size next time. */
      if (trs_disk_debug_flags & DISKDEBUG_REALSIZE) {
	debug("real_write not found: side %d tk %d sec %d size 0%d phytk %d\n",
	      state.curside, state.track, state.sector, d->u.real.size_code,
	      d->phytrack*d->real_step);
      }
#if SIZERETRY
      d->u.real.size_code = (d->u.real.size_code + 1) % 4;
#endif
    }
    if (raw_cmd.reply[1] & 0x81) state.status |= TRSDISK_NOTFOUND;
    if (raw_cmd.reply[1] & 0x20) {
      state.status |= TRSDISK_CRCERR;
      if (!(raw_cmd.reply[2] & 0x20)) state.status |= TRSDISK_NOTFOUND;
    }
    if (raw_cmd.reply[1] & 0x10) state.status |= TRSDISK_LOSTDATA;
    if (raw_cmd.reply[1] & 0x02) {
      state.status |= TRSDISK_WRITEPRT;
      d->writeprot = 1;
    } else {
      d->writeprot = 0;
    }
    if (raw_cmd.reply[2] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[2] & 0x13) state.status |= TRSDISK_NOTFOUND;
  }
  state.bytecount = 0;
  trs_disk_drq_interrupt(0);
  state.status |= TRSDISK_BUSY;
  if (trs_event_scheduled() == trs_disk_lostdata) {
    trs_cancel_event();
  }
  trs_schedule_event(trs_disk_done, 0, 512);
#else
  trs_disk_unimpl(state.currcommand, "write real floppy");
#endif
}

void
real_readadr(void)
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int i, new_status;

  state.status = 0;
  new_status = 0;
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_INTR;
  i = 0;
  raw_cmd.cmd[i++] = state.density ? 0x4a : 0x0a;
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd_count = i;
  raw_cmd.data = NULL;
  raw_cmd.length = 0;
  state.bytecount = 0;
  if (ioctl(fileno(d->file), FDRAWCMD, &raw_cmd) < 0) {
    real_error(d, raw_cmd.flags, "readadr");
    new_status |= TRSDISK_NOTFOUND;
  } else {
    real_ok(d); /* premature? */
    if (raw_cmd.reply[1] & 0x85) new_status |= TRSDISK_NOTFOUND;
    if (raw_cmd.reply[1] & 0x20) new_status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[1] & 0x10) new_status |= TRSDISK_LOSTDATA;
    if (raw_cmd.reply[2] & 0x40) {
      if (state.controller == TRSDISK_P1771) {
        new_status |= TRSDISK_1771_FA;
      } else {
        new_status |= TRSDISK_1791_F8;
      }
    }
    if (raw_cmd.reply[2] & 0x20) new_status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[2] & 0x13) new_status |= TRSDISK_NOTFOUND;
    if ((new_status & TRSDISK_NOTFOUND) == 0) {
      state.status = TRSDISK_BUSY;
      trs_schedule_event(trs_disk_firstdrq, new_status, 64);
      memcpy(d->u.real.buf, &raw_cmd.reply[3], 4);
      d->u.real.buf[4] = d->u.real.buf[5] = 0; /* CRC not emulated */
      state.bytecount = 6;
      d->u.real.size_code = d->u.real.buf[3]; /* update hint */
      return;
    }
  }
  state.last_readadr = -1;
  /* Sector not found; fail */
  state.status = TRSDISK_BUSY;
  trs_schedule_event(trs_disk_done, new_status, 200000*z80_state.clockMHz);
#else
  trs_disk_unimpl(state.currcommand, "read address on real floppy");
#endif
}

void
real_readtrk(void)
{
  trs_disk_unimpl(state.currcommand, "read track on real floppy");
}

void
real_writetrk(void)
{
#if __linux
  DiskState *d = &disk[state.curdrive];
  struct floppy_raw_cmd raw_cmd;
  int gap3;
  unsigned i;
  state.status = 0;

  /* Compute a usable gap3 */
  /* Constants based on IBM format as explained in "The floppy user guide"
     by Michael Haardt, Alain Knaff, and David C. Niemi */
  /* The formulas and constants are not factored out, in case some of
     those that are the same now need to change when I learn more. */
  if (state.density) {
    /* MFM recording */
    if (d->inches == 5) {
      /* 5" DD = 250 kHz MFM */
      gap3 = (TRKSIZE_DD - 161 - /*slop*/16)/(d->u.real.fmt_nbytes / 4)
	- 62 - (128 << d->u.real.size_code) - /*slop*/2;
    } else {
      /* 8" DD = 5" HD = 500 kHz MFM */
      gap3 = (TRKSIZE_8DD - 161 - /*slop*/16)/(d->u.real.fmt_nbytes / 4)
	- 62 - (128 << d->u.real.size_code) - /*slop*/2;
    }
  } else {
    /* FM recording */
    if (d->inches == 5) {
      /* 5" SD = 250 kHz FM (125 kbps) */
      gap3 = (TRKSIZE_SD - 99 - /*slop*/16)/(d->u.real.fmt_nbytes / 4)
	- 33 - (128 << d->u.real.size_code) - /*slop*/2;
    } else {
      /* 8" SD = 5" HD operated in FM = 500 kHz FM (250 kbps) */
      gap3 = (TRKSIZE_8SD - 99 - /*slop*/16)/(d->u.real.fmt_nbytes / 4)
	- 33 - (128 << d->u.real.size_code) - /*slop*/2;
    }
  }
  if (gap3 < 1) {
    error("gap3 too small");
    gap3 = 1;
  } else if (gap3 > 0xff) {
    gap3 = 0xff;
  }

  /* Do the actual write track */
  memset(&raw_cmd, 0, sizeof(raw_cmd));
  raw_cmd.rate = real_rate(d);
  raw_cmd.flags = FD_RAW_WRITE | FD_RAW_INTR;
  i = 0;
  raw_cmd.cmd[i++] = 0x0d | (state.density ? 0x40 : 0x00);
  raw_cmd.cmd[i++] = state.curside ? 4 : 0;
  raw_cmd.cmd[i++] = d->u.real.size_code;
  raw_cmd.cmd[i++] = d->u.real.fmt_nbytes / 4;
  raw_cmd.cmd[i++] = gap3;
  raw_cmd.cmd[i++] = d->u.real.fmt_fill;
  raw_cmd.cmd_count = i;
  raw_cmd.data = (void*) d->u.real.buf;
  raw_cmd.length = d->u.real.fmt_nbytes;

  if (trs_disk_debug_flags & DISKDEBUG_GAPS) {
    debug("real_writetrk size 0%d secs %d gap3 %d fill 0x%02x hex data ",
	  d->u.real.size_code, d->u.real.fmt_nbytes/4, gap3,
	  d->u.real.fmt_fill);
    for (i = 0; i < d->u.real.fmt_nbytes; i += 4) {
      debug("%02x%02x%02x%02x ", d->u.real.buf[i], d->u.real.buf[i + 1],
	    d->u.real.buf[i + 2], d->u.real.buf[i + 3]);
    }
    debug("\n");
  }

  if (ioctl(fileno(d->file), FDRAWCMD, &raw_cmd) < 0) {
    real_error(d, raw_cmd.flags, "writetrk");
    state.status |= TRSDISK_WRITEFLT;
  } else {
    real_ok(d); /* premature? */
    if (raw_cmd.reply[1] & 0x85) state.status |= TRSDISK_NOTFOUND;
    if (raw_cmd.reply[1] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[1] & 0x10) state.status |= TRSDISK_LOSTDATA;
    if (raw_cmd.reply[1] & 0x02) {
      state.status |= TRSDISK_WRITEPRT;
      d->writeprot = 1;
    } else {
      d->writeprot = 0;
    }
    if (raw_cmd.reply[2] & 0x20) state.status |= TRSDISK_CRCERR;
    if (raw_cmd.reply[2] & 0x13) state.status |= TRSDISK_NOTFOUND;
  }
  state.bytecount = 0;
  trs_disk_drq_interrupt(0);
  state.status |= TRSDISK_BUSY;
  if (trs_event_scheduled() == trs_disk_lostdata) {
    trs_cancel_event();
  }
  trs_schedule_event(trs_disk_done, 0, 512);
#else
  trs_disk_unimpl(state.currcommand, "write track on real floppy");
#endif
}

#if 0
int trs_diskset_save(const char *filename)
{
  int i;
  FILE *f = fopen(filename, "w");

  if (f == NULL) {
    error("failed to save Disk Set: %s: %s", filename, strerror(errno));
    return -1;
  }

  for (i = 0; i < 8; i++) {
    fputs(trs_disk_getfilename(i), f);
    fputc('\n', f);
  }
  for (i = 0; i < 4; i++) {
    fputs(trs_hard_getfilename(i), f);
    fputc('\n', f);
  }
  for (i = 0; i < 8; i++) {
    fputs(stringy_get_name(i), f);
    fputc('\n', f);
  }
  fclose(f);
  return 0;
}

int trs_diskset_load(const char *filename)
{
  char diskname[FILENAME_MAX];
  int i;
  FILE *f = fopen(filename, "r");

  if (f == NULL) {
    error("failed to load Disk Set: %s: %s", filename, strerror(errno));
    return -1;
  }

  for (i = 0; i < 8; i++) {
    if (fgets(diskname, FILENAME_MAX, f) == NULL)
      continue;
    if (strlen(diskname) != 0) {
      diskname[strlen(diskname) - 1] = 0;
      if (diskname[0])
        trs_disk_insert(i, diskname);
    }
  }
  for (i = 0; i < 4; i++) {
    if (fgets(diskname, FILENAME_MAX, f) == NULL)
      continue;
    if (strlen(diskname) != 0) {
      diskname[strlen(diskname) - 1] = 0;
      if (diskname[0])
        trs_hard_attach(i, diskname);
    }
  }
  for (i = 0; i < 8; i++) {
    if (fgets(diskname, FILENAME_MAX, f) == NULL)
      continue;
    if (strlen(diskname) != 0) {
      diskname[strlen(diskname) - 1] = 0;
      if (diskname[0])
        stringy_insert(i, diskname);
    }
  }
  fclose(f);
  return 0;
}
#endif

#if 0
static void trs_fdc_save(FILE *file, FDCState *fdc)
{
  trs_save_uint8(file, &fdc->status, 1);
  trs_save_uint8(file, &fdc->track, 1);
  trs_save_uint8(file, &fdc->sector, 1);
  trs_save_uint8(file, &fdc->data, 1);
  trs_save_uint8(file, &fdc->currcommand, 1);
  trs_save_int(file, &fdc->lastdirection, 1);
  trs_save_int(file, &fdc->bytecount, 1);
  trs_save_int(file, &fdc->format, 1);
  trs_save_int(file, &fdc->format_bytecount, 1);
  trs_save_int(file, &fdc->format_sec, 1);
  trs_save_int(file, &fdc->format_gapcnt, 1);
  trs_save_int(file, fdc->format_gap, 5);
  trs_save_uint16(file, &fdc->crc, 1);
  trs_save_uint32(file, &fdc->curdrive, 1);
  trs_save_int(file, &fdc->curside, 1);
  trs_save_int(file, &fdc->density, 1);
  trs_save_uint8(file, &fdc->controller, 1);
  trs_save_int(file, &fdc->last_readadr, 1);
  trs_save_uint64(file, &fdc->motor_timeout, 1);
}

static void trs_fdc_load(FILE *file, FDCState *fdc)
{
  trs_load_uint8(file, &fdc->status, 1);
  trs_load_uint8(file, &fdc->track, 1);
  trs_load_uint8(file, &fdc->sector, 1);
  trs_load_uint8(file, &fdc->data, 1);
  trs_load_uint8(file, &fdc->currcommand, 1);
  trs_load_int(file, &fdc->lastdirection, 1);
  trs_load_int(file, &fdc->bytecount, 1);
  trs_load_int(file, &fdc->format, 1);
  trs_load_int(file, &fdc->format_bytecount, 1);
  trs_load_int(file, &fdc->format_sec, 1);
  trs_load_int(file, &fdc->format_gapcnt, 1);
  trs_load_int(file, fdc->format_gap, 5);
  trs_load_uint16(file, &fdc->crc, 1);
  trs_load_uint32(file, &fdc->curdrive, 1);
  trs_load_int(file, &fdc->curside, 1);
  trs_load_int(file, &fdc->density, 1);
  trs_load_uint8(file, &fdc->controller, 1);
  trs_load_int(file, &fdc->last_readadr, 1);
  trs_load_uint64(file, &fdc->motor_timeout, 1);
}

static void trs_save_sectorid(FILE *file, SectorId *id)
{
  trs_save_uint8(file, &id->track, 1);
  trs_save_uint8(file, &id->sector, 1);
  trs_save_uint8(file, &id->flags, 1);
}

static void trs_load_sectorid(FILE *file, SectorId *id)
{
  trs_load_uint8(file, &id->track, 1);
  trs_load_uint8(file, &id->sector, 1);
  trs_load_uint8(file, &id->flags, 1);
}

static void trs_save_jv3state(FILE *file, JV3State *jv3)
{
  int i;

  trs_save_int(file, jv3->free_id, 4);
  trs_save_int(file, &jv3->last_used_id, 1);
  trs_save_int(file, &jv3->nblocks, 1);
  trs_save_int(file, &jv3->sorted_valid, 1);
  for (i = 0; i < JV3_SECSMAX + 1; i++)
    trs_save_sectorid(file, &jv3->id[i]);
  trs_save_int(file, jv3->offset, JV3_SECSMAX + 1);
  trs_save_short(file, jv3->sorted_id, JV3_SECSMAX + 1);
  for (i = 0; i < MAXTRACKS; i++)
    trs_save_short(file, jv3->track_start[i], JV3_SIDES);
}

static void trs_load_jv3state(FILE *file, JV3State *jv3)
{
  int i;

  trs_load_int(file, jv3->free_id, 4);
  trs_load_int(file, &jv3->last_used_id, 1);
  trs_load_int(file, &jv3->nblocks, 1);
  trs_load_int(file, &jv3->sorted_valid, 1);
  for (i = 0; i < JV3_SECSMAX + 1; i++)
    trs_load_sectorid(file, &jv3->id[i]);
  trs_load_int(file, jv3->offset, JV3_SECSMAX + 1);
  trs_load_short(file, jv3->sorted_id, JV3_SECSMAX + 1);
  for (i = 0; i < MAXTRACKS; i++)
    trs_load_short(file, jv3->track_start[i], JV3_SIDES);
}

static void trs_save_dmkstate(FILE *file, DMKState *dmk)
{
  trs_save_int(file, &dmk->ntracks, 1);
  trs_save_int(file, &dmk->tracklen, 1);
  trs_save_int(file, &dmk->nsides, 1);
  trs_save_int(file, &dmk->sden, 1);
  trs_save_int(file, &dmk->ignden, 1);
  trs_save_int(file, &dmk->curtrack, 1);
  trs_save_int(file, &dmk->curside, 1);
  trs_save_int(file, &dmk->curbyte, 1);
  trs_save_int(file, &dmk->nextidam, 1);
  trs_save_uint8(file, dmk->buf, DMK_TRACKLEN_MAX);
}

static void trs_load_dmkstate(FILE *file, DMKState *dmk)
{
  trs_load_int(file, &dmk->ntracks, 1);
  trs_load_int(file, &dmk->tracklen, 1);
  trs_load_int(file, &dmk->nsides, 1);
  trs_load_int(file, &dmk->sden, 1);
  trs_load_int(file, &dmk->ignden, 1);
  trs_load_int(file, &dmk->curtrack, 1);
  trs_load_int(file, &dmk->curside, 1);
  trs_load_int(file, &dmk->curbyte, 1);
  trs_load_int(file, &dmk->nextidam, 1);
  trs_load_uint8(file, dmk->buf, DMK_TRACKLEN_MAX);
}

static void trs_save_realstate(FILE *file, RealState *real)
{
  trs_save_int(file, &real->rps, 1);
  trs_save_int(file, &real->size_code, 1);
  trs_save_int(file, &real->empty, 1);
  trs_save_int(file, (int *)&real->empty_timeout, 1);
  trs_save_uint32(file, &real->fmt_nbytes, 1);
  trs_save_int(file, &real->fmt_fill, 1);
  trs_save_uint8(file, real->buf, MAXSECSIZE);
}

static void trs_load_realstate(FILE *file, RealState *real)
{
  trs_load_int(file, &real->rps, 1);
  trs_load_int(file, &real->size_code, 1);
  trs_load_int(file, &real->empty, 1);
  trs_load_int(file, (int *)&real->empty_timeout, 1);
  trs_load_uint32(file, &real->fmt_nbytes, 1);
  trs_load_int(file, &real->fmt_fill, 1);
  trs_load_uint8(file, real->buf, MAXSECSIZE);
}

static void trs_save_diskstate(FILE *file, DiskState *d)
{
  int file_not_null = (d->file != NULL);

  trs_save_int(file, &d->writeprot, 1);
  trs_save_int(file, &d->phytrack, 1);
  trs_save_int(file, &d->emutype, 1);
  trs_save_int(file, &d->real_step, 1);
  trs_save_int(file, &file_not_null, 1);
  trs_save_filename(file, d->filename);
  if (d->emutype == JV3)
    trs_save_jv3state(file, &d->u.jv3);
  else if (d->emutype == REAL)
    trs_save_realstate(file, &d->u.real);
  else
    trs_save_dmkstate(file, &d->u.dmk);
}

static void trs_load_diskstate(FILE *file, DiskState *d)
{
  int file_not_null;

  trs_load_int(file, &d->writeprot, 1);
  trs_load_int(file, &d->phytrack, 1);
  trs_load_int(file, &d->emutype, 1);
  trs_load_int(file, &d->real_step, 1);
  trs_load_int(file, &file_not_null, 1);
  if (file_not_null)
    d->file = (FILE *) 1;
  else
    d->file = NULL;
  trs_load_filename(file, d->filename);
  if (d->emutype == JV3)
    trs_load_jv3state(file, &d->u.jv3);
  else if (d->emutype == REAL)
    trs_load_realstate(file, &d->u.real);
  else
    trs_load_dmkstate(file, &d->u.dmk);
}

void trs_disk_save(FILE *file)
{
  int i;

  trs_save_int(file, &trs_disk_nocontroller, 1);
  trs_save_int(file, &trs_disk_doubler, 1);
  trs_save_float(file, &trs_disk_holewidth, 1);
  trs_save_int(file, &trs_disk_truedam, 1);
  trs_save_int(file, &trs_disk_debug_flags, 1);

  trs_fdc_save(file, &state);
  trs_fdc_save(file, &other_state);
  for (i = 0; i < NDRIVES; i++) {
    trs_save_diskstate(file, &disk[i]);
  }
}

void trs_disk_load(FILE *file)
{
  int i;

  for (i = 0; i < NDRIVES; i++) {
    if (disk[i].file != NULL)
      fclose(disk[i].file);
  }
  trs_load_int(file, &trs_disk_nocontroller, 1);
  trs_load_int(file, &trs_disk_doubler, 1);
  trs_load_float(file, &trs_disk_holewidth, 1);
  trs_load_int(file, &trs_disk_truedam, 1);
  trs_load_int(file, &trs_disk_debug_flags, 1);
  trs_fdc_load(file, &state);
  trs_fdc_load(file, &other_state);
  for (i = 0; i < NDRIVES; i++) {
    trs_load_diskstate(file, &disk[i]);
     if (disk[i].file != NULL) {
      disk[i].file = fopen(disk[i].filename, "rb+");
      if (disk[i].file == NULL) {
        disk[i].file = fopen(disk[i].filename, "rb");
        if (disk[i].file == NULL) {
          error("failed to load disk%d: %s: %s", i, disk[i].filename,
              strerror(errno));
          disk[i].emutype = NONE;
          disk[i].writeprot = 0;
          disk[i].filename[0] = 0;
          continue;
        }
        disk[i].writeprot = 1;
      } else {
        disk[i].writeprot = 0;
      }
    }
  }
}
#endif

#endif // CONFIG_TRS_IO_MODEL_1
