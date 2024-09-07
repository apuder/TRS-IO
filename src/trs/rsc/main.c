
#include "retrostore.h"
#include "load.h"
#include "trs-lib.h"

#define BOOT_LOADER_ADDR 0x4300
#include "../../loader/cmd/loader_cmd.c"


static void copy_boot_loader()
{
  int i;
  uint8_t* p = (uint8_t*) BOOT_LOADER_ADDR;
  for (i = 0; i < loader_cmd_bin_len; i++) {
    *p++ = loader_cmd_bin[i];
  }
}

void load_cmd(uint16_t id) {
  __asm
    jp BOOT_LOADER_ADDR + 3 ; HL will hold id
  __endasm;
}

static void download_and_run(int idx)
{
  wnd_popup("Downloading...");
  copy_boot_loader();
  load_cmd(idx);
}

static uint8_t check() {
  static bool first_time = true;
  uint8_t status;
  uint16_t version;
  
  if (first_time) {
    wnd_popup("Scanning...");
  }
  first_time = false;

  status = trs_io_status();
  
  if (status == TRS_IO_STATUS_NO_TRS_IO) {
    wnd_popup("No TRS-IO card found!");
    while(1);
  }

  get_retrostore_version(&version);
  if ((version >> 8) != RS_CLIENT_VERSION_MAJOR) {
    wnd_popup("Incompatible RetroStore card version!");
    while(1);
  }
  
  return status;
}
  
  
#define MENU_BROWSE 0
#define MENU_SEARCH 1
#define MENU_LOAD 2
#define MENU_WIFI 3
#define MENU_HELP 4
#define MENU_STATUS 5
#define MENU_EXIT 6

static menu_item_t main_menu_items[] = {
  MENU_ITEM(MENU_BROWSE, "Browse RetroStore"),
  MENU_ITEM(MENU_SEARCH, "Search RetroStore"),
  MENU_ITEM(MENU_WIFI, "Configure WiFi"),
  MENU_ITEM(MENU_STATUS, "Status"),
  MENU_ITEM(MENU_HELP, "Help"),
  MENU_ITEM(MENU_EXIT, "Exit"),
  MENU_ITEM_END
};

static menu_t main_menu = {
  .title = "RetroStore",
  .items = main_menu_items
};


static menu_item_t main_menu_with_xray_items[] = {
  MENU_ITEM(MENU_BROWSE, "Browse RetroStore"),
  MENU_ITEM(MENU_SEARCH, "Search RetroStore"),
  MENU_ITEM(MENU_LOAD, "Load XRAY state"),
  MENU_ITEM(MENU_WIFI, "Configure WiFi"),
  MENU_ITEM(MENU_STATUS, "Status"),
  MENU_ITEM(MENU_HELP, "Help"),
  MENU_ITEM(MENU_EXIT, "Exit"),
  MENU_ITEM_END
};

static menu_t main_menu_with_xray = {
  .title = "RetroStore",
  .items = main_menu_with_xray_items
};


static menu_item_t main_menu_wifi_not_needed_items[] = {
  MENU_ITEM(MENU_BROWSE, "Browse RetroStore"),
  MENU_ITEM(MENU_SEARCH, "Search RetroStore"),
  MENU_ITEM(MENU_STATUS, "Status"),
  MENU_ITEM(MENU_HELP, "Help"),
  MENU_ITEM(MENU_EXIT, "Exit"),
  MENU_ITEM_END
};

static menu_t main_menu_wifi_not_needed = {
  .title = "RetroStore",
  .items = main_menu_wifi_not_needed_items
};


static menu_item_t main_menu_not_connected_items[] = {
  MENU_ITEM(MENU_WIFI, "Configure WiFi"),
  MENU_ITEM(MENU_STATUS, "Status"),
  MENU_ITEM(MENU_HELP, "Help"),
  MENU_ITEM(MENU_EXIT, "Exit"),
  MENU_ITEM_END
};

static menu_t main_menu_not_connected = {
  .title = "Offline",
  .items = main_menu_not_connected_items
};


static window_t wnd;

int main() {
  int idx;
  bool show_from_left = false;
  bool run = true;
  menu_t* the_menu;
  
  init_window(&wnd, 0, 0, 0, 0);

  while (run) {
    switch (check()) {
    case RS_STATUS_WIFI_NOT_NEEDED:
      the_menu = &main_menu_wifi_not_needed;
      break;
    case RS_STATUS_WIFI_CONNECTED:
      if (has_xray_support()) {
        the_menu = &main_menu_with_xray;
      } else {
        the_menu = &main_menu;
      }
      break;
    default:
      the_menu = &main_menu_not_connected;
      break;
    }
  
    switch (menu(the_menu, show_from_left, true)) {
    case MENU_BROWSE:
      wnd_popup("Loading...");
      idx = browse_retrostore(&wnd);
      if (idx == LIST_ABORT) {
        break;
      }
      download_and_run(idx);
      break;
    case MENU_SEARCH:
      idx = search_retrostore(&wnd);
      if (idx == LIST_ABORT) {
        break;
      }
      download_and_run(idx);
      break;
    case MENU_LOAD:
      load_xray_state();
      break;
    case MENU_WIFI:
      configure_wifi();
      break;
    case MENU_HELP:
      help();
      break;
    case MENU_STATUS:
      status();
      break;
    case MENU_ABORT:
    case MENU_EXIT:
      run = false;
      break;
    }
    show_from_left = true;
  }

  return 0;
}
