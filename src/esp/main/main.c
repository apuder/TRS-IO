
#include "io.h"
#include "led.h"
#include "button.h"
#include "wifi.h"
#include "ota.h"
#include "storage.h"
#include "esp_task.h"
#include "driver/gpio.h"
#include "esp_event.h"

#include "retrostore.h"
#include "backend.h"


void app_main(void)
{
  init_io();
  init_led();
  init_button();
  init_storage();

  if (is_button_pressed()) {
    switch_to_factory();
  }
  
  init_ota();
  init_wifi();

  while (true) {
    while (!is_button_pressed()) {
      vTaskDelay(1);
    }
    int64_t then = esp_timer_get_time();
    while (is_button_pressed()) {
      vTaskDelay(1);
    }
    int64_t now = esp_timer_get_time();
    if ((now - then) / (1000 * 1000) >= 3) {
      storage_erase();
      esp_restart();
    }
  }
}

