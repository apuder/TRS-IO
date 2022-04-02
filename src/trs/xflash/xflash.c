#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <trs-lib.h>
#include "flash.h"


static unsigned char buf[256];
static unsigned char buf2[256];

static struct {
  bool is_m3;
  bool has_trs_io;
  bool has_fpga;
  uint8_t version_major;
  uint8_t version_minor;
  unsigned long mfg_dev_id;
  const char* mfg_dev_id_str;
} status;

#define MENU_FLASH 0
#define MENU_STATUS 1
#define MENU_HELP 2
#define MENU_EXIT 3

static menu_item_t main_menu_items[] = {
  MENU_ITEM(MENU_FLASH, "Flash/Verify FPGA"),
  MENU_ITEM(MENU_STATUS, "Status"),
  MENU_ITEM(MENU_HELP, "Help"),
  MENU_ITEM(MENU_EXIT, "Exit"),
  MENU_ITEM_END
};

static menu_t main_menu = {
  .title = "XFLASH",
  .items = main_menu_items
};

static bool option_flash = true;
static bool option_verify = false;

static char path[20 + 1] = {""};

static form_item_t my_form_items[] = {
	FORM_ITEM_CHECKBOX("Flash", &option_flash, NULL),
	FORM_ITEM_CHECKBOX("Verify", &option_verify, NULL),
	FORM_ITEM_INPUT("Filename", path, sizeof(path), 0, NULL),
	FORM_ITEM_END
};

static form_t my_form = {
	.title = "Flash/Verify",
	.form_items = my_form_items
};

static window_t wnd;

static void help()
{
  set_screen_to_background();
  init_window(&wnd, 0, 3, 0, 0);
  header("Help");
  wnd_print(&wnd, "XFLASH flashes the firmware of the Cmod-A7 FPGA module on the TRS-IO board. ");
  wnd_print(&wnd, "The binary of the firmware needs to copied to the SMB share or SD card. ");
  wnd_print(&wnd, "Make sure to use the correct firmware version depending on the Cmod-A7 (15-T or 35-T). ");
  wnd_print(&wnd, "This utility allows you to flash and verify to the firmware to the FPGA's SPI flash storage.\n\n");
  wnd_print(&wnd, "Details at: https://github.com/apuder/TRS-IO");
  screen_show(false);
  get_key();
}

static void init_status()
{
  status.is_m3 = is_m3();
  status.has_trs_io = trs_io_status() != TRS_IO_STATUS_NO_TRS_IO;
  status.has_fpga = false;
  status.version_major = 0xff;
  status.version_minor = 0xff;
  status.mfg_dev_id = 0;
  status.mfg_dev_id_str = "-";

  if (status.has_trs_io && !status.is_m3) {
    trs_io_get_fpga_version(&status.version_major, &status.version_minor);
    status.has_fpga = (status.version_major != 0xff) && (status.version_minor != 0xff);
    if (status.has_fpga) {
      status.mfg_dev_id = flashReadMfdDevId();
      status.mfg_dev_id_str = flashMfdDevIdStr(status.mfg_dev_id);
    }
  }
}

static void show_status()
{
  set_screen_to_background();
  init_window(&wnd, 0, 3, 0, 0);
  header("Status");
  wnd_print(&wnd, "TRS model            : %s\n", status.is_m3 ? "MIII/4" : "M1");
  wnd_print(&wnd, "TRS-IO card present  : %s\n", status.has_trs_io ? "Yes" : "No");
  wnd_print(&wnd, "FPGA present         : %s\n", status.has_fpga ? "Yes" : "No");
  wnd_print(&wnd, "FPGA firmware version: ");
  if (status.has_fpga) {
    wnd_print(&wnd, "v%d.%d\n", status.version_major, status.version_minor);
  } else {
    wnd_print(&wnd, "-\n");
  }
  wnd_print(&wnd, "SPI Flash Device ID  : ");
  if (status.has_fpga) {
    wnd_print(&wnd, "%06lX\n", status.mfg_dev_id);
  } else {
    wnd_print(&wnd, "-\n");
  }
  wnd_print(&wnd, "SPI Flash Device Name: %s", status.mfg_dev_id_str ? status.mfg_dev_id_str : "Unknown");
  screen_show(false);
  get_key();
}

static bool flash()
{
  unsigned long addr = 0;
  int verify_errors = 0;
  int cnt;
  int8_t fd;

  init_window(&wnd, 0, 3, 0, 0);

  if(form(&my_form, false) == FORM_ABORT) {
    return false;
  }

  if (!status.has_fpga) {
    wnd_popup("No FPGA present!");
    return true;
  }

  if (!option_flash && !option_verify) {
    wnd_popup("Need to select at least one option!");
    return true;
  }

  if(trs_io_status() == TRS_IO_STATUS_NO_TRS_IO) {
    wnd_popup("TRS-IO not found");
    return true;
  }

  fd = trs_fs_open(path, FA_OPEN_EXISTING | FA_READ);
  if(fd < 0) {
    wnd_popup("Error opening file: %s", trs_fs_get_error(trs_io_err));
    return true;
  }

  wnd_cls(&wnd);
  wnd_print(&wnd, "Opened '%s'\n", path);

  trs_io_set_led(false, false, true, false, false);

  do {
    // if the address is the start of a sector then erase the sector
    if((addr & (FLASH_SECTOR_SIZE - 1)) == 0) {
      wnd_print(&wnd, "\n%06lX: ", addr);
      if(option_flash) {
        wnd_print(&wnd, "E");
        flashSectorErase(addr);
      }
    }

    wnd_print(&wnd, "R");
    if (trs_fs_read(fd, buf, sizeof(buf), &cnt) < 0) {
      break;
    }

    if(option_flash) {
      wnd_print(&wnd, "W");
      flashWrite(addr, buf, cnt);
    }

    if(option_verify) {
      wnd_print(&wnd, "V");
      flashRead(addr, buf2, cnt);
      if(memcmp(buf, buf2, cnt) != 0) {
        verify_errors++;
        wnd_print(&wnd, "!");
      }
    } 

    addr += cnt;

    // if <256 bytes read then this is end of file
    if(cnt < 256) {
      break;
    }

    if(scan_key() == KEY_BREAK) {
      wnd_print(&wnd, "\nAbort!");
      break;
    }
  } while(!(cnt < 256));
  wnd_print(&wnd, "\n");

  trs_io_set_led(false, false, false, false, false);
  trs_fs_close(fd);
  wnd_print(&wnd, "%s %ld bytes\n", (option_flash ? "Wrote" : "Read"), addr);
  if(option_verify) {
    wnd_print(&wnd, "%d verify errors detected\n", verify_errors);
  }
  return true;
}

int main()
{
  bool show_from_left = false;
  uint8_t status;
  
  init_trs_lib();
  init_status();

  while(true) {
    status = menu(&main_menu, show_from_left, true);
    if (status == MENU_ABORT || status == MENU_EXIT) {
      break;
    }
    switch (status) {
    case MENU_FLASH:
      if (flash()) {
        get_key();
      }
      break;
    case MENU_STATUS:
      show_status();
      break;
    case MENU_HELP:
      help();
      break;
    }
    show_from_left = true;
  }

  exit_trs_lib();

  return 0;
}
