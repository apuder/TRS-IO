
#include "button.h"
#include "driver/gpio.h"
#include "esp_event.h"

#if defined(CONFIG_TRS_IO_MODEL_1)
#define GPIO_BUTTON GPIO_NUM_39
#elif defined(CONFIG_TRS_IO_PP)
#define GPIO_BUTTON GPIO_NUM_35
#define GPIO_RESET_BUTTON GPIO_NUM_0
#else
#define GPIO_BUTTON GPIO_NUM_22
#endif

#define ESP_INTR_FLAG_DEFAULT 0

#define BUTTON_LONG_PRESS BIT0
#define BUTTON_SHORT_PRESS BIT1

bool is_status_button_pressed()
{
#if defined(CONFIG_TRS_IO_MODEL_1) || defined(CONFIG_TRS_IO_PP)
  return (GPIO.in1.data & (1 << (GPIO_BUTTON - 32))) == 0;
#else
  return (GPIO.in & (1 << GPIO_BUTTON)) == 0;
#endif
}

#ifdef CONFIG_TRS_IO_PP
bool is_reset_button_pressed()
{
  return (GPIO.in & (1 << GPIO_RESET_BUTTON)) == 0;
}
#endif

typedef bool (*button_pressed_t)(void);

typedef struct {
  volatile uint8_t button_status;
  button_pressed_t is_button_pressed;
} button_t;


static button_t status_button = {0, is_status_button_pressed};
#ifdef CONFIG_TRS_IO_PP
static button_t reset_button = {0, is_reset_button_pressed};
#endif

bool is_status_button_long_press()
{
  bool yes = (status_button.button_status & BUTTON_LONG_PRESS) != 0;
  status_button.button_status &= ~ BUTTON_LONG_PRESS;
  return yes;
}

bool is_status_button_short_press()
{
  bool yes = (status_button.button_status & BUTTON_SHORT_PRESS) != 0;
  status_button.button_status &= ~ BUTTON_SHORT_PRESS;
  return yes;
}

#ifdef CONFIG_TRS_IO_PP
bool is_reset_button_long_press()
{
  bool yes = (reset_button.button_status & BUTTON_LONG_PRESS) != 0;
  reset_button.button_status &= ~ BUTTON_LONG_PRESS;
  return yes;
}

bool is_reset_button_short_press()
{
  bool yes = (reset_button.button_status & BUTTON_SHORT_PRESS) != 0;
  reset_button.button_status &= ~ BUTTON_SHORT_PRESS;
  return yes;
}
#endif

#ifndef TRS_IO_BUTTON_ONLY_AT_STARTUP
static void IRAM_ATTR isr_button(void* arg)
{
  static int64_t then = INT64_MAX;
  
  button_t* button = (button_t*) arg;

  if (button->is_button_pressed()) {
    then = esp_timer_get_time();
  } else {
    int64_t now = esp_timer_get_time();
    int64_t delta_ms = (now - then) / 1000;
    then = INT64_MAX;
    if (delta_ms < 50) {
      // Bounce
      return;
    }
    if (delta_ms < 500) {
      button->button_status |= BUTTON_SHORT_PRESS;
    }
    if (delta_ms > 3000) {
      button->button_status |= BUTTON_LONG_PRESS;
    }
  }
}
#endif

void init_button()
{
  gpio_config_t gpioConfig;

  // Configure push button
  gpioConfig.pin_bit_mask = (1ULL << GPIO_BUTTON);
  gpioConfig.mode = GPIO_MODE_INPUT;
#if defined(CONFIG_TRS_IO_MODEL_1) || defined(CONFIG_TRS_IO_PP)
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
#else
  gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
#endif
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
#ifdef TRS_IO_BUTTON_ONLY_AT_STARTUP
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
#else
  gpioConfig.intr_type = GPIO_INTR_ANYEDGE;
#endif
  gpio_config(&gpioConfig);

#ifndef TRS_IO_BUTTON_ONLY_AT_STARTUP
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(GPIO_BUTTON, isr_button, &status_button);
#endif

#ifdef CONFIG_TRS_IO_PP
  gpioConfig.pin_bit_mask = (1ULL << GPIO_RESET_BUTTON);
  gpio_config(&gpioConfig);
  gpio_isr_handler_add(GPIO_RESET_BUTTON, isr_button, &reset_button);
#endif
}
