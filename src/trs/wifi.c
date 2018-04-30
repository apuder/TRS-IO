
#include "wifi.h"

static char ssid[32 + 1];
static char passwd[32 + 1];

static screen_t screen[] = {
  { SCREEN_TYPE_TEXT, 0, 0, .u.text =   "SSID:     "},
  { SCREEN_TYPE_INPUT, -1, -1, .u.input.len = 32, .u.input.buf = ssid},
  { SCREEN_TYPE_TEXT, 0, 1, .u.text = "Password: "},
  { SCREEN_TYPE_INPUT, -1, -1, .u.input.len = 32, .u.input.buf = passwd},
  { SCREEN_TYPE_END }
  };

static window_t wnd;

void init_wifi()
{
  init_window(&wnd, 0, 3, 0, 0);
  show_splash_screen(&wnd, "WiFi");
  show_screen(screen, &wnd);
}
