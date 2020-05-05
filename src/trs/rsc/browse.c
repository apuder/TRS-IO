
#include "retrostore.h"
#include "browse.h"
#include "list.h"
#include "header.h"
#include "key.h"
#include "inout.h"
#include "esp.h"

#define MAX_SEARCH_TERMS_LEN 50

static char search_terms[MAX_SEARCH_TERMS_LEN + 1] = "";

static form_t form_search[] = {
  { FORM_TYPE_TEXT, 0, 1, .u.text =   "Search: "},
  { FORM_TYPE_INPUT, -1, -1, .u.input.len = MAX_SEARCH_TERMS_LEN,
    .u.input.buf = search_terms},
  { FORM_TYPE_END }
};

static list_t list_apps;

static char response[1024];

static void set_query(const char* query)
{
  int i = 0;
  
  out(TRS_IO_PORT, RETROSTORE_MODULE_ID);
  out(TRS_IO_PORT, RS_CMD_SET_QUERY);
  while (query != NULL && query[i] != '\0') {
    out(TRS_IO_PORT, query[i++]);
  }
  out(TRS_IO_PORT, 0);

  wait_for_esp();
}

static bool get_response(uint8_t cmd, uint16_t idx, const char** resp)
{
  int i;
  uint8_t ch;
  
  out(TRS_IO_PORT, RETROSTORE_MODULE_ID);
  out(TRS_IO_PORT, cmd);
  out(TRS_IO_PORT, idx & 0xff);
  out(TRS_IO_PORT, idx >> 8);

  wait_for_esp();

  i = 0;
  while (true) {
    if (i == sizeof(response) - 1) {
      break;
    }
    ch = in(TRS_IO_PORT);
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
  do {
    key = get_key();
  } while (key != KEY_BREAK && key != KEY_CLEAR && key != KEY_LEFT &&
           key != KEY_ENTER);
  return (key == KEY_ENTER) ? idx : LIST_EXIT;
}

static uint16_t show_app_list(window_t* wnd,
                              const char* title,
                              const char* query)
{
  bool show_from_left = false;
  uint16_t idx;

  set_query(query);
  init_list(&list_apps, title, get_item);
  while (true) {
    idx = list(wnd, &list_apps, show_from_left, true);
    if (idx == LIST_EXIT) {
      return LIST_EXIT;
    }
    wnd_popup(wnd, "Loading details...");
    idx = show_details(idx);
    if (idx != LIST_EXIT) {
      return idx;
    }
    show_from_left = true;
  }
  return LIST_EXIT;
}

uint16_t browse_retrostore(window_t* wnd)
{
  return show_app_list(wnd, "Browse RetroStore", NULL);
}

uint16_t search_retrostore(window_t* wnd)
{
  if (form(wnd, "Search RetroStore", form_search, false) == FORM_ABORT) {
    return LIST_EXIT;
  }
  wnd_popup(wnd, "Searching...");
  wnd_switch_to_background(wnd);
  wnd_cls(wnd);
  return show_app_list(wnd, "Search Results", search_terms);
}
