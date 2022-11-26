
#include <trs-lib.h>
#include "retrostore.h"

#define MAX_SEARCH_TERMS_LEN 50

static char search_terms[MAX_SEARCH_TERMS_LEN + 1] = "";

static form_item_t form_search_items[] = {
  FORM_ITEM_INPUT("Search", search_terms, MAX_SEARCH_TERMS_LEN, 0, NULL),
  FORM_ITEM_END
};

static form_t form_search = {
	.title = "Search RetroStore",
	.form_items = form_search_items
};

static list_t list_apps;

static char response[1024];

static void set_query(const char* query)
{
  int i = 0;
  
  out31(RETROSTORE_MODULE_ID);
  out31(RS_CMD_SET_QUERY);
  while (query != NULL && query[i] != '\0') {
    out31(query[i++]);
  }
  out31(0);

  wait_for_esp();
}

static bool get_response(uint8_t cmd, uint16_t idx, const char** resp)
{
  int i;
  uint8_t ch;
  
  out31(RETROSTORE_MODULE_ID);
  out31(cmd);
  out31(idx & 0xff);
  out31(idx >> 8);

  wait_for_esp();

  i = 0;
  while (true) {
    if (i == sizeof(response) - 1) {
      break;
    }
    ch = in31();
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

  set_screen_to_background();
  init_window(&wnd, 0, 3, 0, 0);
  if (!get_response(RS_SEND_APP_DETAILS, idx, &details)) {
    return LIST_ABORT;
  }
  descr = details;
  while (*descr != '\n') {
    descr++;
  }
  *descr++ = '\0';
  header(details);
  wnd_print_str(&wnd, descr);
  screen_show(false);
  do {
    key = get_key();
  } while (key != KEY_BREAK && key != KEY_CLEAR && key != KEY_LEFT &&
           key != KEY_ENTER);
  return (key == KEY_ENTER) ? idx : LIST_ABORT;
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
    idx = list(&list_apps, show_from_left, true);
    if (idx == LIST_ABORT) {
      break;
    }
    wnd_popup("Loading details...");
    idx = show_details(idx);
    if (idx != LIST_ABORT) {
      return idx;
    }
    show_from_left = true;
  }
  return LIST_ABORT;
}

uint16_t browse_retrostore(window_t* wnd)
{
  return show_app_list(wnd, "Browse RetroStore", NULL);
}

uint16_t search_retrostore(window_t* wnd)
{
  if (form(&form_search, false) == FORM_ABORT) {
    return LIST_ABORT;
  }
  wnd_popup("Searching...");
  set_screen_to_background();
  wnd_cls(wnd);
  return show_app_list(wnd, "Search Results", search_terms);
}
