#include "retrostore.h"
#include "version.h"

static window_t wnd;

void about()
{
  char* status;
  uint8_t scan_result;
  uint16_t version;
  
  init_window(&wnd, 0, 3, 0, 0);
  header(&wnd, "About");
  wnd_print(&wnd, false, "RetroStore client version: ");
  wnd_print_int(&wnd, RS_CLIENT_VERSION_MAJOR);
  wnd_print(&wnd, false, ".");
  wnd_print_int(&wnd, RS_CLIENT_VERSION_MINOR);
  wnd_cr(&wnd);

  scan_result = scan();
  wnd_print(&wnd, false, "RetroStore card version  : ");
  if (scan_result == RS_STATUS_NO_RETROSTORE_CARD) {
    wnd_print(&wnd, false, "-");
  } else {
    version = get_version();
    wnd_print_int(&wnd, version >> 8);
    wnd_print(&wnd, false, ".");
    wnd_print_int(&wnd, version & 0xff);
  }
  wnd_cr(&wnd);

  switch(scan_result) {
  case RS_STATUS_WIFI_NOT_NEEDED:
    status = "emulation mode";
    break;
  case RS_STATUS_WIFI_CONNECTING:
    status = "WiFi connecting";
    break;
  case RS_STATUS_WIFI_CONNECTED:
    status = "WiFi connected";
    break;
  case RS_STATUS_WIFI_NOT_CONNECTED:
    status = "WiFi not connected";
    break;
  case RS_STATUS_WIFI_NOT_CONFIGURED:
    status = "WiFi not configured";
    break;
  case RS_STATUS_NO_RETROSTORE_CARD:
    status = "not present";
    break;
  default:
    status = "";
  }
  wnd_print(&wnd, false, "RetroStore card status   : ");
  wnd_print(&wnd, false, status);

  wnd_show(&wnd, false);
  get_key();
}
