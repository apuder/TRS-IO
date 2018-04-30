
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

void jumpToBoot() {
  __asm
    jp BOOT_LOADER_ADDR
  __endasm;
}


static menu_t main_menu;

static const char* items[] = {
  "Browse RetroStore",
  "Search RetroStore",
  "Configure WiFi",
  "Help",
  "About"};

static const char* menu_get_title() {
  return "RetroStore";
}

static uint16_t menu_get_count() {
  return sizeof(items) / sizeof(const char*);
}

const char* menu_get_item(uint16_t idx) {
  return items[idx];
}


static window_t wnd;

void main() {
  bool show_from_left = false;
  init_window(&wnd, 0, 0, 0, 0);
  init_menu(&main_menu, menu_get_title, menu_get_count, menu_get_item);
  
  while (true) {
    uint16_t m = menu(&wnd, &main_menu, show_from_left);
    switch(m) {
    case 2: // WiFi
      init_wifi();
      break;
    }
    show_from_left = true;
  }
}
