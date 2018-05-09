
#include "button.h"
#include "driver/gpio.h"

void init_button()
{
  gpio_config_t gpioConfig;

  // Configure push button
  gpioConfig.pin_bit_mask = GPIO_SEL_22;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
}

bool is_button_pressed()
{
  return (GPIO.in & (1 << GPIO_NUM_22)) == 0;
}
