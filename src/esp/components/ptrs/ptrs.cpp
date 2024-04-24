

#include "ptrs.h"
#include "cass.h"
#include "roms.h"
#include <trs-lib.h>
#include <fabgl.h>

#define MENU_CONFIGURE 0
#define MENU_ROMS 1
#define MENU_STATUS 2
#define MENU_RESET 3
#define MENU_HELP 4
#define MENU_EXIT 5


static menu_item_t main_menu_items[] = {
  MENU_ITEM(MENU_CONFIGURE, "Configure"),
  MENU_ITEM(MENU_ROMS, "ROMs"),
  MENU_ITEM(MENU_STATUS, "Status"),
  MENU_ITEM(MENU_RESET, "Reset Settings"),
  MENU_ITEM(MENU_HELP, "Help"),
  MENU_ITEM(MENU_EXIT, "Exit"),
  MENU_ITEM_END
};

static menu_t main_menu = {
  .title = "TRS-IO++",
  .items = main_menu_items
};



void configure_pocket_trs(bool is_80_cols)
{
  bool reboot = false;
  bool show_from_left = false;
  uint8_t status;

  init_trs_lib(is_80_cols);

  while(!reboot) {
    status = menu(&main_menu, show_from_left, true);
    if (status == MENU_ABORT || status == MENU_EXIT) {
      break;
    }
    switch (status) {
    case MENU_CONFIGURE:
      reboot = configure_ptrs_settings();
      break;
    case MENU_ROMS:
      configure_roms();
      break;
    case MENU_STATUS:
      ptrs_status();
      break;
    }
    show_from_left = true;
  }

  if (reboot) {
    wnd_popup("Rebooting TRS-IO++...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }

  exit_trs_lib();

  if (reboot) {
    esp_restart();
  }
}

#if 0
void configure_pocket_trsx()
{
  bool show_from_left = false;
  bool exit = false;
  uint8_t mode = trs_screen.getMode();

  screenBuffer = new ScreenBuffer(mode);
  trs_screen.push(screenBuffer);
  screenBuffer->copyBufferFrom(screenBuffer->getNext());

  ScreenBuffer* backgroundBuffer = new ScreenBuffer(mode);
  trs_screen.push(backgroundBuffer);

  set_screen(screenBuffer->getBuffer(), backgroundBuffer->getBuffer(),
	     screenBuffer->getWidth(), screenBuffer->getHeight());

  set_screen_callback(screen_update);
  set_keyboard_callback(get_next_key);

  while (!exit) {
    uint8_t action = menu(&main_menu, show_from_left, true);
    switch (action) {
    case MENU_CONFIGURE:
      configure();
      break;
    case MENU_CALIBRATE:
      calibrate();
      break;
    case MENU_STATUS:
      status();
      break;
    case MENU_RESET:
      SettingsBase::reset();
      storage_erase();
      esp_restart();
      break;
    case MENU_HELP:
      help();
      break;
    case MENU_EXIT:
    case MENU_ABORT:
      exit = true;
      break;
    }
    show_from_left = true;
  }

  // Copy original screen content of the TRS emulation
  // to the background buffer
  backgroundBuffer->copyBufferFrom(screenBuffer->getNext());
  screen_show(true);
  trs_screen.pop();
  trs_screen.pop();
}
#endif

void init_ptrs()
{
  init_cass();
}