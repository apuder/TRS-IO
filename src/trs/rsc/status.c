
#include "retrostore.h"
#include "version.h"
#include "trs-lib.h"

static window_t wnd;

static void print_details(window_t* wnd, uint8_t cmd, const char* label)
{
  char ch;
  
  wnd_print(wnd, label);
  out31(TRS_IO_CORE_MODULE_ID);
  out31(cmd);
  wait_for_esp();
  while (true) {
    ch = in31();
    if (ch == '\0') {
      break;
    }
    wnd_print(wnd, "%c", ch);
  }
  wnd_cr(wnd);
}

void status()
{
  char* stat;
  uint8_t scan_result;
  uint8_t revision;
  uint16_t version;
  
  set_screen_to_background();
  init_window(&wnd, 0, 3, 0, 0);
  header("Status");

  scan_result = trs_io_status();
  get_trs_io_version(&revision, &version);

  wnd_print(&wnd, "TRS-IO hardware revision    : ");
  if (scan_result == TRS_IO_STATUS_NO_TRS_IO) {
    wnd_print(&wnd, "not present");
  } else {
    wnd_print(&wnd, "%d", revision);
  }
  wnd_cr(&wnd);
  
  wnd_print(&wnd, "TRS-IO core module version  : ");
  if (scan_result == TRS_IO_STATUS_NO_TRS_IO) {
    wnd_print(&wnd, "-");
  } else {
    wnd_print(&wnd, "%d.%d", version >> 8, version & 0xff);
  }
  wnd_cr(&wnd);

  get_retrostore_version(&version);
  wnd_print(&wnd, "RetroStore module version   : ");
  if (scan_result == TRS_IO_STATUS_NO_TRS_IO) {
    wnd_print(&wnd, "-");
  } else {
    wnd_print(&wnd, "%d.%d", version >> 8, version & 0xff);
  }
  wnd_cr(&wnd);

  wnd_print(&wnd, "RetroStore client version   : ");
  wnd_print(&wnd, "%d.%d", RS_CLIENT_VERSION_MAJOR, RS_CLIENT_VERSION_MINOR);
  wnd_cr(&wnd);

  switch(scan_result) {
  case RS_STATUS_WIFI_NOT_NEEDED:
    stat = "emulation mode";
    break;
  case RS_STATUS_WIFI_CONNECTING:
    stat = "WiFi connecting";
    break;
  case RS_STATUS_WIFI_CONNECTED:
    stat = "WiFi connected";
    break;
  case RS_STATUS_WIFI_NOT_CONNECTED:
    stat = "WiFi not connected";
    break;
  case RS_STATUS_WIFI_NOT_CONFIGURED:
    stat = "WiFi not configured";
    break;
  case RS_STATUS_NO_RETROSTORE_CARD:
    stat = "not present";
    break;
  default:
    stat = "";
  }
  wnd_print(&wnd, "TRS-IO online status        : ");
  wnd_print(&wnd, stat);
  wnd_cr(&wnd);

  print_details(&wnd, TRS_IO_SEND_WIFI_SSID, "WiFi SSID: ");
  print_details(&wnd, TRS_IO_SEND_WIFI_IP, "WiFi IP  : ");
  
  screen_show(false);
  get_key();
}
