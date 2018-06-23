
#include "retrostore.h"

#define BOOT_LOADER_ADDR 0x4300
#include "../boot/boot.c"


static void copy_boot_loader()
{
  int i;
  uint8_t* p = (uint8_t*) BOOT_LOADER_ADDR;
  for (i = 0; i < boot_bin_len; i++) {
    *p++ = boot_bin[i];
  }
}

void load_cmd(uint16_t id) {
  __asm
    pop bc
    pop hl
    jp BOOT_LOADER_ADDR + 3
  __endasm;
}

static uint8_t scan(window_t* wnd) {
  static bool first_time = true;
  uint16_t i = 0;
  uint8_t status;

  wnd_switch_to_foreground(wnd);
  if (first_time) {
    wnd_popup(wnd, "Scanning...");
  }
  first_time = false;
  
  out(RS_PORT, RS_SEND_STATUS);

  while (++i != 0) {
    status = in(RS_PORT);
    if (status == RS_STATUS_NO_RETROSTORE_CARD) {
      wnd_popup(wnd, "No RetroStore card found!");
      while(1);
    }
    if (status == RS_STATUS_WIFI_NOT_NEEDED ||
        status == RS_STATUS_WIFI_CONNECTED) {
      break;
    }
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

  copy_boot_loader();
  
  init_window(&wnd, 0, 0, 0, 0);

  while (true) {
    switch (scan(&wnd)) {
    case RS_STATUS_WIFI_NOT_NEEDED:
      the_menu = &main_menu_wifi_not_needed;
      break;
    case RS_STATUS_WIFI_CONNECTED:
      the_menu = &main_menu;
      break;
    defaul:
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
    case MENU_ABOUT:
      break;
    }
    show_from_left = true;
  }
}
