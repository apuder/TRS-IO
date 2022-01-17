
#include "trs-io.h"
#include "frehd.h"
#include "io.h"
#include "button.h"
#include "led.h"
#include "storage.h"
#include "spi.h"
#include "wifi.h"
#include "trs-fs.h"
#include "esp_task.h"
#include "esp_event.h"
#include "tcpip.h"
#include "retrostore.h"
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"


#define ESP_REQ GPIO_NUM_34
#define ESP_DONE GPIO_NUM_27


#define ADJUST(x) ((x) < 32 ? (x) : (x) - 32)

#define MASK_ESP_REQ (1 << ADJUST(ESP_REQ))
#define MASK64_ESP_REQ (1ULL << ESP_REQ)
#define MASK_ESP_DONE (1 << ADJUST(ESP_DONE))
#define MASK64_ESP_DONE (1ULL << ESP_DONE)


static volatile bool DRAM_ATTR trigger_trs_io_action = false;


#define IO_CORE1_ENABLE_INTR BIT0
#define IO_CORE1_DISABLE_INTR BIT1
#define TRS_IO_DONE BIT2

static volatile bool io_task_started = false;
static volatile uint8_t DRAM_ATTR intr_event = 0;
static volatile bool DRAM_ATTR intr_enabled = true;

static volatile int16_t DRAM_ATTR printer_data = -1;

void io_core1_enable_intr() {
#if 0
  if (!io_task_started) {
    return;
  }
  intr_event |= IO_CORE1_ENABLE_INTR;
  while (intr_event & IO_CORE1_ENABLE_INTR) ;
#endif
}

void io_core1_disable_intr() {
#if 0
  if (!io_task_started) {
    return;
  }
  intr_event |= IO_CORE1_DISABLE_INTR;
  while (intr_event & IO_CORE1_DISABLE_INTR) ;
#endif
}

static inline uint8_t dbus_read()
{
  if (!intr_enabled) {
    //portENABLE_INTERRUPTS();
  }
  uint8_t d = spi_dbus_read();
  if (!intr_enabled) {
    //portDISABLE_INTERRUPTS();
  }
  return d;
}

static inline void dbus_write(uint8_t d)
{
  if (!intr_enabled) {
    //portENABLE_INTERRUPTS();
  }
  spi_dbus_write(d);
  if (!intr_enabled) {
    //portDISABLE_INTERRUPTS();
  }
}

static inline void trs_io_read() {
  if (!trigger_trs_io_action) {
    uint8_t data = dbus_read();
    if (!TrsIO::outZ80(data)) {
      trigger_trs_io_action = true;
    }
  }
}

static inline void trs_io_write() {
  uint8_t d = trigger_trs_io_action ? 0xff : TrsIO::inZ80();
  dbus_write(d);
}

static inline void frehd_read(uint8_t port) {
  uint8_t data = dbus_read();
  frehd_out(port, data);
}

static inline void frehd_write(uint8_t port) {
  uint32_t d = frehd_in(port);
  dbus_write(d);
}

static volatile bool DRAM_ATTR esp_req_triggered = false;
static volatile bool DRAM_ATTR trigger_trs_io_done = false;

static void IRAM_ATTR esp_req_isr_handler(void* arg)
{
  esp_req_triggered = true;
}

static void IRAM_ATTR io_task(void* p)
{
  while(true) {
    while (!esp_req_triggered && !trigger_trs_io_done) ;
    if (trigger_trs_io_done) {
      trigger_trs_io_done = false;
      spi_trs_io_done();
      continue;
    }
    esp_req_triggered = false;
    // Read pins [S2..S0,A3..A0]
    const uint8_t s = GPIO.in >> 12;
    switch(s & 0x70) {
    case 0x00:
      trs_io_write();
      break;
    case 0x10:
      trs_io_read();
      break;
    case 0x20:
      frehd_write(s);
      break;
    case 0x30:
      frehd_read(s);
      break;
    }
    
    // Pulse a rising edge for ESP_DONE to mark end of operation
    GPIO.out_w1tc = MASK_ESP_DONE;
    //for(volatile int i = 0; i < 10; i++) __asm("nop;nop;nop;nop;nop;nop;nop;nop;");
    GPIO.out_w1ts = MASK_ESP_DONE;
  }
}

static void action_task(void* p)
{
  // Clear any spurious interrupts
  is_button_short_press();
  is_button_long_press();

  while (true) {
    frehd_check_action();

    if (trigger_trs_io_action) {
      if (printer_data == -1) {
        TrsIO::processInBackground();
        trigger_trs_io_action = false;
        //spi_trs_io_done();
        trigger_trs_io_done = true;
//        intr_event |= TRS_IO_DONE;
      } else {
        char buf[2] = {(char) (printer_data & 0xff), 0};
        trs_printer_write(buf);
        printer_data = -1;
        trigger_trs_io_action = false;
      }
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

  // GPIO pins 12-18 (7 pins) are used for S0-S2 and A0-A4
  gpioConfig.pin_bit_mask = GPIO_SEL_12 | GPIO_SEL_13 | GPIO_SEL_14 |
    GPIO_SEL_15 | GPIO_SEL_16 |
    GPIO_SEL_17 | GPIO_SEL_18;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);

  // Configure ESP_SEL_N
  gpioConfig.pin_bit_mask = MASK64_ESP_REQ;
  gpioConfig.intr_type = GPIO_INTR_POSEDGE;
  gpio_config(&gpioConfig);
  //gpio_install_isr_service(0);
  gpio_isr_handler_add(ESP_REQ, esp_req_isr_handler, NULL);    

  // Configure ESP_DONE
  gpioConfig.pin_bit_mask = MASK64_ESP_DONE;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
  
  // Set ESP_DONE to 0
  gpio_set_level((gpio_num_t) ESP_DONE, 0);

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, tskIDLE_PRIORITY + 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(action_task, "action", 6000, NULL, 1, NULL, 0);
}
