
#include "wifi.h"
#include "inout.h"
#include "retrostore.h"

static char ssid[32 + 1] = "";
static char passwd[32 + 1] = "";

static form_t form_wifi[] = {
  { FORM_TYPE_TEXT, 0, 0, .u.text =   "SSID:     "},
  { FORM_TYPE_INPUT, -1, -1, .u.input.len = 32, .u.input.buf = ssid},
  { FORM_TYPE_TEXT, -1, -1, .u.text = "\nPassword: "},
  { FORM_TYPE_INPUT, -1, -1, .u.input.len = 32, .u.input.buf = passwd},
  { FORM_TYPE_END }
  };

static window_t wnd;

void configure_wifi()
{
  uint16_t i, j;
  
  init_window(&wnd, 0, 0, 0, 0);
  if (form(&wnd, "Configure WiFi", form_wifi, false) == FORM_ABORT) {
    return;
  }
  if (ssid[0] == '\0') {
    // No SSID provided. Exit
    return;
  }
  out(RS_PORT, RS_CMD_CONFIGURE_WIFI);
  i = 0;
  while (ssid[i] != '\0') {
    out(RS_PORT, ssid[i++]);
  }
  out(RS_PORT, '\t');
  i = 0;
  while (passwd[i] != '\0') {
    out(RS_PORT, passwd[i++]);
  }
  out(RS_PORT, '\0');
  wnd_popup(&wnd, "Reconfiguring WiFi...");
  for (i = 0; i < 10; i++) {
    for (j = 0; j < 65000; j++) {
      if(0);
    }
  }
}
