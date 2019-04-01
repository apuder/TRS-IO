
#include "button.h"
#include "storage.h"
#include "driver/gpio.h"
#include "esp_event.h"

#define GPIO_BUTTON GPIO_NUM_22

#define ESP_INTR_FLAG_DEFAULT 0

static void IRAM_ATTR isr_button(void* arg)
{
  static int64_t then;
  
  if (is_button_pressed()) {
    then = esp_timer_get_time();
  } else {
    int64_t now = esp_timer_get_time();
    if ((now - then) / (1000 * 1000) >= 3) {
      storage_erase();
      esp_restart();
    }
  }
}

void init_button()
{
  gpio_config_t gpioConfig;

  // Configure push button
  gpioConfig.pin_bit_mask = (1ULL << GPIO_BUTTON);
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&gpioConfig);
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(GPIO_BUTTON, isr_button, NULL);
}

bool is_button_pressed()
{
  return (GPIO.in & (1 << GPIO_BUTTON)) == 0;
}
