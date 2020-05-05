
#include "retrostore.h"

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
    pop bc
    pop hl
    jp BOOT_LOADER_ADDR + 3
  __endasm;
}

static uint8_t check(window_t* wnd) {
  static bool first_time = true;
  uint8_t status;
  uint16_t version;
  
  wnd_switch_to_foreground(wnd);
  if (first_time) {
    wnd_popup(wnd, "Scanning...");
  }
  first_time = false;

  status = scan();
  
  if (status == RS_STATUS_NO_RETROSTORE_CARD) {
    wnd_popup(wnd, "No RetroStore card found!");
    while(1);
  }

  get_retrostore_version(&version);
  if ((version >> 8) != RS_CLIENT_VERSION_MAJOR) {
    wnd_popup(wnd, "Incompatible RetroStore card version!");
    while(1);
  }
  
  wnd_switch_to_background(wnd);
  return status;
}
  
  
#define MENU_BROWSE 0
#define MENU_SEARCH 1
#define MENU_WIFI 2
#define MENU_HELP 3
#define MENU_ABOUT 4

static menu_item_t main_menu_items[] = {
  {MENU_BROWSE, "Browse RetroStore"},
  {MENU_SEARCH, "Search RetroStore"},
  {MENU_WIFI, "Configure WiFi"},
  {MENU_HELP, "Help"},
  {MENU_ABOUT, "About"}
};

MENU(main_menu, "RetroStore");

static menu_item_t main_menu_wifi_not_needed_items[] = {
  {MENU_BROWSE, "Browse RetroStore"},
  {MENU_SEARCH, "Search RetroStore"},
  {MENU_HELP, "Help"},
  {MENU_ABOUT, "About"}
};

MENU(main_menu_wifi_not_needed, "RetroStore");

static menu_item_t main_menu_not_connected_items[] = {
  {MENU_WIFI, "Configure WiFi"},
  {MENU_HELP, "Help"},
  {MENU_ABOUT, "About"}
};

MENU(main_menu_not_connected, "Offline");

static window_t wnd;

void main() {
  int idx;
  bool show_from_left = false;
  menu_t* the_menu;
  uint8_t status;

  // For M4, turn on MIII memory map. Nop on a MIII
  out(0x84, 0);

  copy_boot_loader();
  
  init_window(&wnd, 0, 0, 0, 0);

  while (true) {
    switch (check(&wnd)) {
    case RS_STATUS_WIFI_NOT_NEEDED:
      the_menu = &main_menu_wifi_not_needed;
      break;
    case RS_STATUS_WIFI_CONNECTED:
      the_menu = &main_menu;
      break;
    default:
      the_menu = &main_menu_not_connected;
      break;
    }
  
    status = menu(&wnd, the_menu, show_from_left, false);
    switch (status) {
    case MENU_BROWSE:
      wnd_popup(&wnd, "Loading...");
      idx = browse_retrostore(&wnd);
      if (idx == LIST_EXIT) {
        break;
      }
      wnd_popup(&wnd, "Downloading...");
      load_cmd(idx);
      break;
    case MENU_SEARCH:
      idx = search_retrostore(&wnd);
      if (idx == LIST_EXIT) {
        break;
      }
      wnd_popup(&wnd, "Downloading...");
      load_cmd(idx);
      break;
    case MENU_WIFI:
      configure_wifi();
      break;
    case MENU_HELP:
      help();
      break;
    case MENU_ABOUT:
      about();
      break;
    }
    show_from_left = true;
  }
}
