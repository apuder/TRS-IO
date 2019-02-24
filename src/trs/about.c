#include "retrostore.h"
#include "version.h"

static window_t wnd;

static void print_details(window_t* wnd, uint8_t cmd, const char* label)
{
  char ch[2] = ".";
  
  wnd_print(wnd, false, label);
  out(TRS_IO_PORT, TRS_IO_CORE_MODULE_ID);
  out(TRS_IO_PORT, cmd);
  wait_for_esp();
  while (true) {
    ch[0] = in(TRS_IO_PORT);
    if (ch[0] == '\0') {
      break;
    }
    wnd_print(wnd, false, ch);
  }
  wnd_cr(wnd);
}

void about()
{
  char* status;
  uint8_t scan_result;
  uint8_t revision;
  uint16_t version;
  
  init_window(&wnd, 0, 3, 0, 0);
  header(&wnd, "About");

  scan_result = scan();
  get_trs_io_version(&revision, &version);

  wnd_print(&wnd, false, "TRS-IO hardware revision    : ");
  if (scan_result == RS_STATUS_NO_RETROSTORE_CARD) {
    wnd_print(&wnd, false, "not present");
  } else {
    wnd_print_int(&wnd, revision);
  }
  wnd_cr(&wnd);
  
  wnd_print(&wnd, false, "TRS-IO core module version  : ");
  if (scan_result == RS_STATUS_NO_RETROSTORE_CARD) {
    wnd_print(&wnd, false, "-");
  } else {
    wnd_print_int(&wnd, version >> 8);
    wnd_print(&wnd, false, ".");
    wnd_print_int(&wnd, version & 0xff);
  }
  wnd_cr(&wnd);

  get_retrostore_version(&version);
  wnd_print(&wnd, false, "RetroStore module version   : ");
  if (scan_result == RS_STATUS_NO_RETROSTORE_CARD) {
    wnd_print(&wnd, false, "-");
  } else {
    wnd_print_int(&wnd, version >> 8);
    wnd_print(&wnd, false, ".");
    wnd_print_int(&wnd, version & 0xff);
  }
  wnd_cr(&wnd);

  wnd_print(&wnd, false, "RetroStore client version   : ");
  wnd_print_int(&wnd, RS_CLIENT_VERSION_MAJOR);
  wnd_print(&wnd, false, ".");
  wnd_print_int(&wnd, RS_CLIENT_VERSION_MINOR);
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
  wnd_print(&wnd, false, "TRS-IO online status        : ");
  wnd_print(&wnd, false, status);
  wnd_cr(&wnd);

  print_details(&wnd, TRS_IO_SEND_WIFI_SSID, "WiFi SSID: ");
  print_details(&wnd, TRS_IO_SEND_WIFI_IP, "WiFi IP  : ");
  
  wnd_show(&wnd, false);
  get_key();
}
