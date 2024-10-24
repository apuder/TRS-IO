#include <string.h>
#include "led.h"
#include "utils.h"
#include "rst.h"
#include "version.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define CHECK_OTA(err) if ((err) != ESP_OK) return;

static const char* TAG = "OTA";


void switch_to_factory()
{
  esp_partition_iterator_t pi;

  pi = esp_partition_find(ESP_PARTITION_TYPE_APP,
                          ESP_PARTITION_SUBTYPE_APP_FACTORY,
                          "factory");
  if (pi == NULL) {
    ESP_LOGE(TAG, "Failed to find factory partition");
  } else {
    const esp_partition_t* factory = esp_partition_get(pi);
    esp_partition_iterator_release(pi);
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(factory));
    reboot_trs_io();
  }
}
