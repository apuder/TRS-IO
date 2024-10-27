
#include "led.h"
#include "button.h"
#include "rst.h"
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

  // Set LED to blue
  set_led(false, false, true, false, true);

  pi = esp_partition_find(ESP_PARTITION_TYPE_APP,
                          ESP_PARTITION_SUBTYPE_APP_FACTORY,
                          "factory");
  if (pi == NULL) {
    ESP_LOGE(TAG, "Failed to find factory partition");
    set_led(false, false, false, false, false);
  } else {
    const esp_partition_t* factory = esp_partition_get(pi);
    esp_partition_iterator_release(pi);
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(factory));
    do {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    } while (is_status_button_pressed());
    set_led(false, false, false, false, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    reboot_trs_io();
  }
}
