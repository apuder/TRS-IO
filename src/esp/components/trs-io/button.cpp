
#include "button.h"
#include "driver/gpio.h"
#include "esp_event.h"

#define GPIO_BUTTON GPIO_NUM_22

#define ESP_INTR_FLAG_DEFAULT 0

#define BUTTON_LONG_PRESS BIT0
#define BUTTON_SHORT_PRESS BIT1

static volatile uint8_t button_status = 0;

bool is_button_long_press()
{
  bool yes = (button_status & BUTTON_LONG_PRESS) != 0;
  button_status &= ~ BUTTON_LONG_PRESS;
  return yes;
}

bool is_button_short_press()
{
  bool yes = (button_status & BUTTON_SHORT_PRESS) != 0;
  button_status &= ~ BUTTON_SHORT_PRESS;
  return yes;
}

#ifndef TRS_IO_BUTTON_ONLY_AT_STARTUP
static void IRAM_ATTR isr_button(void* arg)
{
  static int64_t then;
  
  if (is_button_pressed()) {
    then = esp_timer_get_time();
  } else {
    int64_t now = esp_timer_get_time();
    int64_t delta_ms = (now - then) / 1000;
    if (delta_ms < 50) {
      // Bounce
      return;
    }
    if (delta_ms < 1000) {
      button_status |= BUTTON_SHORT_PRESS;
    }
    if (delta_ms > 3000) {
      button_status |= BUTTON_LONG_PRESS;
    }
  }
}
#endif

void init_button()
{
#if 0
  gpio_config_t gpioConfig;

  // Configure push button
  gpioConfig.pin_bit_mask = (1ULL << GPIO_BUTTON);
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
#ifdef TRS_IO_BUTTON_ONLY_AT_STARTUP
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
#else
  gpioConfig.intr_type = GPIO_INTR_ANYEDGE;
#endif
  gpio_config(&gpioConfig);
#ifndef TRS_IO_BUTTON_ONLY_AT_STARTUP
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(GPIO_BUTTON, isr_button, NULL);
#endif
#endif
}

bool is_button_pressed()
{
#if 0
  return (GPIO.in & (1 << GPIO_BUTTON)) == 0;
#else
  return 0;
#endif
}
