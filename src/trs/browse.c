
#include "retrostore.h"
#include "browse.h"
#include "list.h"
#include "header.h"
#include "key.h"
#include "inout.h"

static list_t list_apps;

static const char* get_title() {
  return "RetroApps";
}

static char response[1024];

static bool get_response(uint8_t cmd, uint16_t idx, const char** resp)
{
  int i = 0;
  uint8_t ch;

  out(RS_PORT, cmd);
  out(RS_PORT, idx & 0xff);
  out(RS_PORT, idx >> 8);
  while (in(0xe0) & 8) ;

  while (true) {
    if (i == sizeof(response) - 1) {
      break;
    }
    ch = in(RS_PORT);
    if (ch == '\0') {
      break;
    }
    response[i++] = ch;
  }

  response[i] = '\0';
  *resp = response;
  return i != 0;
}


static bool get_item(uint16_t idx, const char** item)
{
  return get_response(RS_SEND_APP_TITLE, idx, item);
}

static uint16_t show_details(uint16_t idx)
{
  window_t wnd;
  char* details;
  char* descr;
  char key;
  
  init_window(&wnd, 0, 3, 0, 0);
  if (!get_response(RS_SEND_APP_DETAILS, idx, &details)) {
    return LIST_EXIT;
  }
  descr = details;
  while (*descr != '\n') {
    descr++;
  }
  *descr++ = '\0';
  header(&wnd, details);
  wnd_print(&wnd, false, descr);
  wnd_show(&wnd, false);
  key = get_key();
  return (key == KEY_BREAK || key == KEY_CLEAR) ? LIST_EXIT : idx;
}

uint16_t browse_retrostore(window_t* wnd)
{
  bool show_from_left = false;
  uint16_t idx;
  init_list(&list_apps, get_title, get_item);
  while (true) {
    idx = list(wnd, &list_apps, show_from_left);
    if (idx == LIST_EXIT || show_details(idx) != LIST_EXIT) {
      return idx;
    }
    show_from_left = true;
  }
  return LIST_EXIT;
}
