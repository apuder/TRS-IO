
#include "io.h"
#include "led.h"
#include "button.h"
#include "wifi.h"

#include "storage.h"
#include "driver/gpio.h"
#include "esp_event.h"

#include "../../boot/boot.c"
//#include "../../boot/cosmic.c"
#include "../../boot/defense.c"


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
    uint8_t command = read_byte();
    if (command == 0) {
      write_bytes(boot_bin, boot_bin_len);
    } else if (command == 1) {
      //            write_bytes(cosmic_cmd, cosmic_cmd_len);
      write_bytes(defense_cmd, defense_cmd_len);
    } else {
      printf("Illegal command: %d", command);
    }
  }
}

