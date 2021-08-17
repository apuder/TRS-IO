
#include <stdio.h>
#include <string.h>
#include "trs_hard.h"
#include "trs_disk.h"
#include "trs_extra.h"
#include "action.h"
#include "fileio.h"

//#define RUN_FILE_IO_TESTS
void test_file_io();

/* Global variables */
Drive state_d[TRS_HARD_MAXDRIVES];
FIL state_file2;
#if EXTRA_IM_SUPPORT
#if _USE_FASTSEEK
DWORD im_stbl[FAST_SEEK_LEN];
#endif
FIL im_file;
BYTE im_buf[0x80];
#endif
BYTE sector_buffer[MAX_SECTOR_SIZE];
#if EXTRA_IM_SUPPORT
image_t im[8];
#endif

BYTE extra_buffer[EXTRA_SIZE];
DIR_ state_dir;
FILINFO state_fno;
UCHAR state_dir_open;
UCHAR state_file2_open;
UCHAR fs_mounted;
UCHAR cur_unit;
UCHAR led_count;

BYTE action_flags;
BYTE action_type;
BYTE action_status;
UCHAR state_busy;
UCHAR state_status;
UCHAR state_present;
UCHAR state_control;
UCHAR state_error;
UCHAR state_seccnt;
UCHAR state_secnum;
USHORT state_cyl;
UCHAR state_drive;
UCHAR state_head;
UCHAR state_wp;
UCHAR state_command;
USHORT state_bytesdone;
UCHAR state_secsize;
USHORT state_secsize16;
UCHAR state_command2;
UCHAR state_error2;
USHORT state_size2;
USHORT state_bytesdone2;
UCHAR state_romdone;
UCHAR state_rom;
UCHAR val_1F;
UCHAR crc7;
USHORT crc16;
UCHAR foo;

void pic_init(void)
{
  /* globals init */
  val_1F = 0x1F;
  action_flags = 0;
  fs_mounted = FS_NOT_MOUNTED;
  state_file2_open = 0;
  state_dir_open = 0;
  for (int i = 0; i < TRS_HARD_MAXDRIVES; i++) {
    state_d[i].filename[0] = '\0';
  }
}

void frehd_check_action()
{
  if (action_flags & ACTION_TRS) {
    action_flags &= ~ACTION_TRS;
    trs_action();
  }
}

void init_frehd()
{
  pic_init();
  frehd_init();
#ifdef CONFIG_TRS_IO_MODEL_1
  trs_disk_init(1);
#endif

#ifdef RUN_FILE_IO_TESTS
  test_file_io();
#endif

  /* Set READY status */
  state_busy = 0;
  update_status(TRS_HARD_READY);  
}

void frehd_main(void)
{
  while (1) {
    /* hard drive and extra functions actions */
    frehd_check_action();
  }
}
