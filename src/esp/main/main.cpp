
#include "io.h"
#include "led.h"
#include "button.h"
#include "wifi.h"
#include "ota.h"
#include "storage.h"
#include "event.h"
#include "esp_event.h"

#include "retrostore.h"
#include "backend.h"
#include "trs-io.h"
#include "ntp_sync.h"


extern "C" {
  void app_main(void);
}

#define SMB_URL "smb_url"
#define SMB_USER "smb_user"
#define SMB_PASSWD "smb_passwd"

void app_main(void)
{
  init_events();
  init_trs_io();
  init_led();
  init_button();
  init_storage();

  if (is_button_pressed()) {
#ifdef TRS_IO_BUTTON_ONLY_AT_STARTUP
    storage_erase();
#else
    switch_to_factory();
#endif
  }
  
  storage_set_str(SMB_URL, "smb://192.168.1.10/TRS-80");
  storage_set_str(SMB_USER, "TRS-IO");
  storage_set_str(SMB_PASSWD, "tandyassembly--");
  
  init_ota();
  init_wifi();

  init_time();

  vTaskDelete(NULL);
}

