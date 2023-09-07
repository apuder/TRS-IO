
#include "wifi.h"
#include "version-ptrs.h"
#include "version.h"
#include "trs-lib.h"
#include "trs-fs.h"
#include "settings.h"


static window_t wnd;

void ptrs_status()
{
  set_screen_to_background();
  init_window(&wnd, 0, 3, 0, 0);
  header("Status");

  wnd_print(&wnd, "PocketTRS version: %d.%d", POCKET_TRS_VERSION_MAJOR, POCKET_TRS_VERSION_MINOR);
  wnd_cr(&wnd);

  wnd_print(&wnd, "TRS-IO version   : %d.%d", TRS_IO_VERSION_MAJOR, TRS_IO_VERSION_MINOR);
  wnd_cr(&wnd);
  wnd_cr(&wnd);

  const char* status;
  switch(*get_wifi_status()) {
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
  wnd_print(&wnd, "TRS-IO online status: %s", status);
  wnd_cr(&wnd);

  wnd_print(&wnd, "WiFi SSID           : %s", settings_get_wifi_ssid().c_str());
  wnd_cr(&wnd);

  wnd_print(&wnd, "WiFi IP             : %s", get_wifi_ip());
  wnd_cr(&wnd);
  wnd_cr(&wnd);

  wnd_print(&wnd, "SMB status: ");
  const char* smb_err = get_smb_err_msg();
  wnd_print(&wnd, smb_err == NULL ? "SMB connected" : smb_err);

  if (trs_fs_has_sd_card_reader()) {
    wnd_cr(&wnd);
    wnd_print(&wnd, "SD status : ");
    const char* posix_err = get_posix_err_msg();
    wnd_print(&wnd, posix_err == NULL ? "Mounted" : posix_err);
  }

  screen_show(false);
  get_key();
}