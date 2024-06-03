
#include "io.h"
#include "led.h"
#include "button.h"
#include "wifi.h"
#include "http.h"
#include "ota.h"
#include "settings.h"
#include "event.h"
#include "esp_event.h"
#include "esp_log.h"

#include "trs-io.h"
#include "trs-fs.h"
#include "spi.h"
#include "keyb.h"
#include "ptrs.h"
#include "xflash.h"

#include "esp_idf_version.h"

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(4, 4, 7)
#error Please use ESP-IDF version 4.4.7
#endif

#define TAG "TRS-IO"

#ifdef CONFIG_TRS_IO_MODEL_1
#define CONFIG "TRS-IO (Model 1)"
#endif
#ifdef CONFIG_TRS_IO_MODEL_3
#define CONFIG "TRS-IO (Model III)"
#endif
#ifdef CONFIG_TRS_IO_PP
#define CONFIG "TRS-IO++"
#endif

extern const char* GIT_REV;
extern const char* GIT_BRANCH;


extern "C" {

void app_main(void)
{
  ESP_LOGI(TAG, "TRS-IO branch=%s, rev=%s", GIT_BRANCH, GIT_REV);
  ESP_LOGI(TAG, "Configured for %s", CONFIG);

  init_events();
  init_trs_io();
  init_button();
  init_settings();

  if (is_status_button_pressed()) {
#ifdef TRS_IO_BUTTON_ONLY_AT_STARTUP
    settings_reset_all();
#else
    switch_to_factory();
#endif
  }
  
  init_ota();
  init_spi();
  init_led();
  init_http();
#ifdef CONFIG_TRS_IO_PP
  init_keyb();
  init_ptrs();
#endif
  init_trs_fs_posix();
  init_wifi();
  init_io();

#ifdef CONFIG_TRS_IO_PP
  init_fpga();
#endif

  vTaskDelete(NULL);
}

} // extern "C"
