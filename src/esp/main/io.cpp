
#include "trs-io.h"
#include "frehd.h"
#include "io.h"
#include "button.h"
#include "led.h"
#include "storage.h"
#include "wifi.h"
#include "trs-fs.h"
#include "esp_task.h"
#include "esp_event_loop.h"
#include "tcpip.h"
#include "retrostore.h"
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"


// GPIO pins 12-19
#define GPIO_DATA_BUS_MASK 0b11111111000000000000

// GPIO(23): ESP_SEL_N (TRS-IO or Frehd)
#define ESP_SEL_N 23
#define ESP_SEL_TRS_IO 39
#define WAIT_N 27
#define IOBUSINT_N 25
#define RD_N 36

#define ADJUST(x) ((x) < 32 ? (x) : (x) - 32)

#define MASK_ESP_SEL_N (1 << ADJUST(ESP_SEL_N))
#define MASK64_ESP_SEL_N (1ULL << ESP_SEL_N)
#define MASK_ESP_SEL_TRS_IO (1 << ADJUST(ESP_SEL_TRS_IO))
#define MASK64_ESP_SEL_TRS_IO (1ULL << ESP_SEL_TRS_IO)
#define MASK_WAIT_N (1 << ADJUST(WAIT_N))
#define MASK64_WAIT_N (1ULL << WAIT_N)
#define MASK_IOBUSINT_N (1 << ADJUST(IOBUSINT_N))
#define MASK64_IOBUSINT_N (1ULL << IOBUSINT_N)
#define MASK_RD_N (1 << ADJUST(RD_N))
#define MASK64_RD_N (1ULL << RD_N)

#define GPIO_OUTPUT_DISABLE(mask) GPIO.enable_w1tc = (mask)

#define GPIO_OUTPUT_ENABLE(mask) GPIO.enable_w1ts = (mask)

static volatile bool trigger_trs_io_action = false;

#define IO_CORE1_ENABLE_INTR BIT0
#define IO_CORE1_DISABLE_INTR BIT1

static volatile bool io_task_started = false;
static volatile uint8_t intr_event = 0;

void io_core1_enable_intr() {
  if (!io_task_started) {
    return;
  }
  intr_event |= IO_CORE1_ENABLE_INTR;
  while (intr_event & IO_CORE1_ENABLE_INTR) ;
}

void io_core1_disable_intr() {
  if (!io_task_started) {
    return;
  }
  intr_event |= IO_CORE1_DISABLE_INTR;
  while (intr_event & IO_CORE1_DISABLE_INTR) ;
}

static inline void trs_io_read() {
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
  if (!trigger_trs_io_action) {
    uint8_t data = GPIO.in >> 12;
    if (!TrsIO::outZ80(data)) {
      trigger_trs_io_action = true;
    }
  }
}

static inline void trs_io_write() {
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
  GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
  uint32_t d = trigger_trs_io_action ? 0xff : TrsIO::inZ80();
  d <<= 12;
  REG_WRITE(GPIO_OUT_W1TS_REG, d);
  d = d ^ GPIO_DATA_BUS_MASK;
  REG_WRITE(GPIO_OUT_W1TC_REG, d);
}

static inline void frehd_read() {
  uint8_t port = GPIO.in1.data & 0x0f;
  uint8_t data = GPIO.in >> 12;
  frehd_out(port, data);
}

static inline void frehd_write() {
  uint8_t port = GPIO.in1.data & 0x0f;
  uint32_t d = frehd_in(port) << 12;

  GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
  REG_WRITE(GPIO_OUT_W1TS_REG, d);
  d = d ^ GPIO_DATA_BUS_MASK;
  REG_WRITE(GPIO_OUT_W1TC_REG, d);
}

static void io_task(void* p)
{
  io_task_started = true;
  portDISABLE_INTERRUPTS();
  
  while(true) {
    // Wait for access to ports 31 or 0xC0-0xCF
    while ((GPIO.in & MASK_ESP_SEL_N) && (intr_event == 0)) ;

    if (intr_event != 0) {
      if (intr_event & IO_CORE1_ENABLE_INTR) {
        portENABLE_INTERRUPTS();
        intr_event &= ~IO_CORE1_ENABLE_INTR;
      } else if (intr_event & IO_CORE1_DISABLE_INTR) {
        portDISABLE_INTERRUPTS();
        intr_event &= ~IO_CORE1_DISABLE_INTR;
      }
      continue;
    }

    if (GPIO.in1.data & MASK_RD_N) {
      // Read data
#ifdef TRS_IO_USE_RETROSTORE_PCB
      trs_io_read();
#else
      if (GPIO.in1.data & MASK_ESP_SEL_TRS_IO) {
        trs_io_read();
      } else {
        frehd_read();
      }
#endif
    } else {
      // Write data
#ifdef TRS_IO_USE_RETROSTORE_PCB
      trs_io_write();
#else
      if (GPIO.in1.data & MASK_ESP_SEL_TRS_IO) {
        trs_io_write();
      } else {
        frehd_write();
      }
#endif
    }
    
    // Release WAIT_N
    GPIO.out_w1ts = MASK_WAIT_N;

    // Wait for ESP_SEL_N to be de-asserted
    while (!(GPIO.in & MASK_ESP_SEL_N)) ;
    
    // Set WAIT_N to 0 for next IO command
    GPIO.out_w1tc = MASK_WAIT_N;

    GPIO_OUTPUT_DISABLE(GPIO_DATA_BUS_MASK);
  }
}

static void action_task(void* p)
{
  while (true) {
    frehd_check_action();

    if (trigger_trs_io_action) {
      TrsIO::processInBackground();
      trigger_trs_io_action = false;
      REG_WRITE(GPIO_OUT_W1TS_REG, MASK_IOBUSINT_N);
    }

    if (is_button_long_press()) {
      storage_erase();
      esp_restart();
    }

    if (is_button_short_press()) {
      // Check Wifi status
      if (*get_wifi_status() == RS_STATUS_WIFI_CONNECTED) {
        set_led(false, true, false, false, false);
      } else {
        set_led(true, false, false, false, false);
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      set_led(false, false, false, false, false);
      vTaskDelay(500 / portTICK_PERIOD_MS);

      // Check SMB status
      if (get_smb_err_msg() == NULL) {
        set_led(false, true, false, false, false);
      } else {
        set_led(true, false, false, false, false);
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      set_led(false, false, false, false, false);      
    }
      
    vTaskDelay(1);
  }
}

void init_io()
{
  init_frehd();

  gpio_config_t gpioConfig;

  // GPIO pins 12-19 (8 pins) are used for data bus
  gpioConfig.pin_bit_mask = GPIO_SEL_12 | GPIO_SEL_13 | GPIO_SEL_14 |
    GPIO_SEL_15 | GPIO_SEL_16 |
    GPIO_SEL_17 | GPIO_SEL_18 | GPIO_SEL_19;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);

  // GPIO pins 32-35 are use for A0-A3
  gpioConfig.pin_bit_mask = GPIO_SEL_32 | GPIO_SEL_33 | GPIO_SEL_34 |
    GPIO_SEL_35;
  gpio_config(&gpioConfig);

  // Configure RD_N
  gpioConfig.pin_bit_mask = MASK64_RD_N;
  gpio_config(&gpioConfig);
  
  // Configure ESP_SEL_N
  gpioConfig.pin_bit_mask = MASK64_ESP_SEL_N;
  gpio_config(&gpioConfig);

  // Configure ESP_SEL_TRS_IO
  gpioConfig.pin_bit_mask = MASK64_ESP_SEL_TRS_IO;
  gpio_config(&gpioConfig);
  
  // Configure IOBUSINT_N & WAIT_N
  gpioConfig.pin_bit_mask = MASK64_IOBUSINT_N | MASK64_WAIT_N;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
  
  // Set IOBUSINT_N to 0
  gpio_set_level((gpio_num_t) IOBUSINT_N, 0);
  
  // Set ESP_WAIT_N to 0
  gpio_set_level((gpio_num_t) WAIT_N, 0);

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, tskIDLE_PRIORITY + 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(action_task, "action", 6000, NULL, 1, NULL, 0);
}
