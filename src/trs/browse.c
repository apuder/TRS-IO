
#include "browse.h"
#include "list.h"
#include "inout.h"

static list_t list_apps;

static const char* get_title() {
  return "RetroApps";
}

static char app_name[64];

static bool get_item(uint16_t idx, const char** name) {
  int i = 0;
  uint8_t ch;

  out(31, 2);
  out(31, idx & 0xff);
  out(31, idx >> 8);
  while (in(0xe0) & 8) ;

  while (true) {
    if (i == sizeof(app_name) - 1) {
      break;
    }
    ch = in(31);
    if (ch == '\0') {
      break;
    }
    app_name[i++] = ch;
  }

  app_name[i] = '\0';
  *name = app_name;
  return i != 0;
}

uint16_t browse_retrostore(window_t* wnd)
{
  uint16_t idx;
  init_list(&list_apps, get_title, get_item);
  idx = list(wnd, &list_apps, false);
  return idx;
}
