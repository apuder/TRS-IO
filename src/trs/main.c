
#include "retrostore.h"

#define BOOT_LOADER_ADDR 0x4300
#include "../boot/boot.c"


void copyBootLoader() {
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

static menu_item_t main_menu_wifi_items[] = {
  {MENU_WIFI, "Configure WiFi"},
  {MENU_HELP, "Help"},
  {MENU_ABOUT, "About"}
};

MENU(main_menu_wifi, "RetroStore");

static window_t wnd;

void main() {
  bool show_from_left = false;
  init_window(&wnd, 0, 0, 0, 0);

  while (true) {
    uint8_t m = menu(&wnd, &main_menu_wifi, show_from_left);
    switch(m) {
    case MENU_WIFI:
      init_wifi();
      break;
    case MENU_ABOUT:
      load_cmd(42);
      break;
    }
    show_from_left = true;
  }
}
