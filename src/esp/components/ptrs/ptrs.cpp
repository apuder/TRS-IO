

#include "ptrs.h"
#include <trs-lib.h>
#include <fabgl.h>

#define MENU_CONFIGURE 0
#define MENU_CALIBRATE 1
#define MENU_STATUS 2
#define MENU_RESET 3
#define MENU_HELP 4
#define MENU_EXIT 5


static menu_item_t main_menu_items[] = {
  MENU_ITEM(MENU_CONFIGURE, "Configure"),
  MENU_ITEM(MENU_STATUS, "Status"),
  MENU_ITEM(MENU_RESET, "Reset Settings"),
  MENU_ITEM(MENU_HELP, "Help"),
  MENU_ITEM(MENU_EXIT, "Exit"),
  MENU_ITEM_END
};

static menu_t main_menu = {
  .title = "PocketTRS",
  .items = main_menu_items
};



void configure_pocket_trs(bool is_80_cols)
{
  bool show_from_left = false;
  uint8_t status;

  init_trs_lib(is_80_cols);

  while(true) {
    status = menu(&main_menu, show_from_left, true);
    if (status == MENU_ABORT || status == MENU_EXIT) {
      break;
    }
    switch (status) {
    case MENU_CONFIGURE:
      configure_ptrs_settings();
      break;
    case MENU_STATUS:
      ptrs_status();
      break;
    }
    show_from_left = true;
  }

  exit_trs_lib();
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
  // Do nothing
}