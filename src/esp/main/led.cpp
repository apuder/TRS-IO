
#include "led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define LED_RED CONFIG_TRS_IO_GPIO_LED_RED
#define LED_GREEN CONFIG_TRS_IO_GPIO_LED_GREEN
#define LED_BLUE CONFIG_TRS_IO_GPIO_LED_BLUE
#define LED_SEL_MASK ((1ULL << LED_RED) | (1ULL << LED_GREEN) | (1ULL << LED_BLUE))

#define BIT_R BIT0
#define BIT_G BIT1
#define BIT_B BIT2

#define BIT_BLINK BIT3
#define BIT_AUTO_OFF BIT4

#define BIT_TRIGGER BIT5

static TaskHandle_t task_handle;
static EventGroupHandle_t event_group;


#ifdef CONFIG_TRS_IO_TEST_LED
static void test_led()
{
  while(1) {
    // First blink twice in white
    set_led(true, true, true, false, false);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    set_led(false, false, false, false, false);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    set_led(true, true, true, false, false);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    set_led(false, false, false, false, false);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Red
    set_led(true, false, false, false, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // Green
    set_led(false, true, false, false, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    // Blue
    set_led(false, false, true, false, false);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
#endif
    

static void led_task(void* p)
{
  TickType_t delay = portMAX_DELAY;
  uint32_t r = 0;
  uint32_t g = 0;
  uint32_t b = 0;
  bool on = false;
  bool auto_off = false;
  
  while(true) {
    EventBits_t bits = xEventGroupWaitBits(event_group,
                                           BIT_R | BIT_G | BIT_B |
                                           BIT_BLINK | BIT_AUTO_OFF |
                                           BIT_TRIGGER,
                                           pdTRUE, // Clear on exit
                                           pdFALSE, // Wait for all bits
                                           delay);
    if (bits != 0) {
      r = (bits & BIT_R) ? 1 : 0;
      g = (bits & BIT_G) ? 1 : 0;
      b = (bits & BIT_B) ? 1 : 0;
      delay = (bits & BIT_BLINK) ? 500 / portTICK_PERIOD_MS : portMAX_DELAY;
      auto_off = bits & BIT_AUTO_OFF;
      if (auto_off) {
        delay = 3000 / portTICK_PERIOD_MS;
      }
      on = true;
    }

    if (on) {
      gpio_set_level((gpio_num_t) LED_RED, r);
      gpio_set_level((gpio_num_t) LED_GREEN, g);
      gpio_set_level((gpio_num_t) LED_BLUE, b);
    } else {
      gpio_set_level((gpio_num_t) LED_RED, 0);
      gpio_set_level((gpio_num_t) LED_GREEN, 0);
      gpio_set_level((gpio_num_t) LED_BLUE, 0);
      if (auto_off) {
        delay = portMAX_DELAY;
      }
    }
    on = !on;
  }
}

void init_led()
{
  gpio_config_t gpioConfig;

  // Configure LED
  gpioConfig.pin_bit_mask = LED_SEL_MASK;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);

  event_group = xEventGroupCreate();
  xEventGroupClearBits(event_group, 0xff);
  xTaskCreatePinnedToCore(led_task, "led", 2048, NULL, 1, &task_handle, 0);
  
  // Turn LED off
  set_led(false, false, false, false, false);
#ifdef CONFIG_TRS_IO_TEST_LED
  test_led();
#endif
}
    

void set_led(bool r, bool g, bool b, bool blink, bool auto_off)
{
  EventBits_t mask = BIT_TRIGGER;

  if (r) {
    mask |= BIT_R;
  }
  if (g) {
    mask |= BIT_G;
  }
  if (b) {
    mask |= BIT_B;
  }
  if (blink) {
    mask |= BIT_BLINK;
  }
  if (auto_off) {
    mask |= BIT_AUTO_OFF;
  }
  xEventGroupSetBits(event_group, mask);
}
