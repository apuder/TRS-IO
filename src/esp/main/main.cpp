
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
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp32/spiram.h"
#include "esp_ota_ops.h"

#include "trs-io.h"
#include "trs-fs.h"
#include "spi.h"
#include "keyb.h"
#include "ptrs.h"
#include "xflash.h"
#include "spiffs.h"

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


static void check()
{
#ifdef CONFIG_TRS_IO_PP
  size_t flash_size = spi_flash_get_chip_size() / (1024 * 1024);
  size_t psram_size = esp_spiram_get_size() / (1024 * 1024);

  if (flash_size == 16) {
    ESP_LOGI(TAG, "Found 16MB flash size");
  } else {
    ESP_LOGE(TAG, "Flash size needs to be configured to 16MB. Found %dMB", flash_size);
  }
  if (psram_size == 8) {
    ESP_LOGI(TAG, "Found 8MB of PSRAM");
  } else {
    ESP_LOGE(TAG, "Need 8MB of PSRAM. Found %dMB", psram_size);
  }

  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        // Assume everything is peachy just because we made it here
        ESP_LOGI(TAG, "Marking OTA update as valid");
        esp_ota_mark_app_valid_cancel_rollback();
      }
  }
  ESP_LOGI(TAG, "Running partition: %s", running->label);
#endif
}

extern "C" {

void app_main(void)
{
  ESP_LOGI(TAG, "TRS-IO branch=%s, rev=%s", GIT_BRANCH, GIT_REV);
  ESP_LOGI(TAG, "Configured for %s", CONFIG);

  // Silence "spi_master: device5 locked the bus" debug messages when
  // accessing the SD card.
  esp_log_level_set("spi_master", ESP_LOG_INFO);

  check();

  init_events();
  init_trs_io();
  init_button();
  init_settings();
  init_spi();
  init_led();
  init_trs_fs_posix();
  init_spiffs();
#ifdef CONFIG_TRS_IO_PP
  init_keyb();
#endif
  init_fpga();
  init_wifi();
  init_io();
  init_http();

  vTaskDelete(NULL);
}

} // extern "C"
