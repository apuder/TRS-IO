
#include "wifi.h"

static char ssid[32 + 1] = "JujuNET";
static char passwd[32 + 1] = "";

static form_t form_wifi[] = {
  { FORM_TYPE_TEXT, 0, 0, .u.text =   "SSID:     "},
  { FORM_TYPE_INPUT, -1, -1, .u.input.len = 32, .u.input.buf = ssid},
  { FORM_TYPE_TEXT, -1, -1, .u.text = "\nPassword: "},
  { FORM_TYPE_INPUT, -1, -1, .u.input.len = 32, .u.input.buf = passwd},
  { FORM_TYPE_END }
  };

static window_t wnd;

void init_wifi()
{
  init_window(&wnd, 0, 3, 0, 0);
  header(&wnd, "WiFi");
  form(&wnd, form_wifi);
}
