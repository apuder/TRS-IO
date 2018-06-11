
#include "retrostore.h"
#include "browse.h"
#include "list.h"
#include "header.h"
#include "key.h"
#include "inout.h"

#define MAX_SEARCH_TERMS_LEN 50

static char search_terms[MAX_SEARCH_TERMS_LEN + 1] = "";
static bool use_search_terms = false;

static form_t form_search[] = {
  { FORM_TYPE_TEXT, 0, 1, .u.text =   "Search: "},
  { FORM_TYPE_INPUT, -1, -1, .u.input.len = MAX_SEARCH_TERMS_LEN,
    .u.input.buf = search_terms},
  { FORM_TYPE_END }
};

static list_t list_apps;

static const char* get_title() {
  return "Browse RetroApps";
}

static char response[1024];

static bool get_response(uint8_t cmd, uint16_t idx, const char** resp)
{
  int i;
  uint8_t ch;

  out(RS_PORT, cmd);
  out(RS_PORT, idx & 0xff);
  out(RS_PORT, idx >> 8);

  if (use_search_terms) {
    i = 0;
    do {
      out(RS_PORT, search_terms[i]);
    } while (search_terms[i++] != '\0');
  } else {
    out(RS_PORT, 0);
  }
  
  while (in(0xe0) & 8) ;

  i = 0;
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

uint16_t browse_retrostore(window_t* wnd, bool use_search_terms_)
{
  bool show_from_left = false;
  uint16_t idx;

  use_search_terms = use_search_terms_;
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

uint16_t search_retrostore(window_t* wnd)
{
  if (form(wnd, "Search RetroApps", form_search, false) == FORM_ABORT) {
    return LIST_EXIT;
  }
  wnd_popup(wnd, "Searching...");
  wnd_switch_to_background(wnd);
  wnd_cls(wnd);
  return browse_retrostore(wnd, true);
}
