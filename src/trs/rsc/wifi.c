
#include "wifi.h"
#include "inout.h"
#include "retrostore.h"
#include "trs-lib.h"

static char ssid[32 + 1] = "";
static char passwd[32 + 1] = "";

static bool ssid_dirty;
static bool passwd_dirty;

static form_item_t form_wifi_items[] = {
  FORM_ITEM_INPUT("SSID", ssid, sizeof(ssid) - 1, 0, &ssid_dirty),
  FORM_ITEM_INPUT("Password", passwd, sizeof(passwd) - 1, 0, &passwd_dirty),
  FORM_ITEM_END
};

static form_t form_wifi = {
	.title = "Configure WiFi",
	.form_items = form_wifi_items
};


static window_t wnd;

void configure_wifi()
{
  uint16_t i, j;
  
  init_window(&wnd, 0, 0, 0, 0);
  if (form(&form_wifi, false) == FORM_ABORT) {
    return;
  }
  if (ssid[0] == '\0') {
    // No SSID provided. Exit
    return;
  }
  if (!ssid_dirty && !passwd_dirty) {
    // User didn't change anything
    return;
  }
  out31(TRS_IO_CORE_MODULE_ID);
  out31(TRS_IO_CMD_CONFIGURE_WIFI);
  i = 0;
  do {
    out31(ssid[i]);
  } while (ssid[i++] != '\0');
  i = 0;
  do {
    out31(passwd[i]);
  } while (passwd[i++] != '\0');
  wnd_popup("Reconfiguring WiFi...");
  for (i = 0; i < 10; i++) {
    for (j = 0; j < 65000; j++) {
      if(0);
    }
  }
}
