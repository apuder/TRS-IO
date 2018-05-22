
#include "io.h"
#include "led.h"
#include "button.h"
#include "wifi.h"
#include "storage.h"
#include "esp_task.h"
#include "driver/gpio.h"
#include "esp_event.h"

#include "retrostore.h"


void app_main(void)
{
  init_io();
  init_led();
  init_button();
  init_storage();

  if (is_button_pressed()) {
    storage_erase();
  }
  init_wifi();

#if 0
  vTaskDelay(5000 / portTICK_PERIOD_MS);
  list_apps(0,5);
#endif
  
  vTaskSuspend(NULL);
  
  while (true) {
#if 0
    set_led(true, false, false, false, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    set_led(false, true, false, false, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    set_led(false, false, true, false, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    continue;
#endif
    //gpio_set_level(GPIO_NUM_25, gpio_get_level(GPIO_NUM_22) ? 0 : 1);
    //io_cycle();
  }
}

