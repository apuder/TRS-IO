
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


// GPIO(23): ESP_SEL_N (TRS-IO or Frehd)
#define ESP_SEL_TRS_IO 39
#define IOBUSINT_N 25
#define ESP_SEL_N 23
#define ESP_WAIT_RELEASE_N 27

#define ESP_READ_N 36

#define ADJUST(x) ((x) < 32 ? (x) : (x) - 32)

#define MASK_ESP_SEL_N (1 << ADJUST(ESP_SEL_N))
#define MASK64_ESP_SEL_N (1ULL << ESP_SEL_N)
#define MASK_ESP_SEL_TRS_IO (1 << ADJUST(ESP_SEL_TRS_IO))
#define MASK64_ESP_SEL_TRS_IO (1ULL << ESP_SEL_TRS_IO)
#define MASK_ESP_WAIT_RELEASE_N (1 << ADJUST(ESP_WAIT_RELEASE_N))
#define MASK64_ESP_WAIT_RELEASE_N (1ULL << ESP_WAIT_RELEASE_N)
#define MASK_IOBUSINT_N (1 << ADJUST(IOBUSINT_N))
#define MASK64_IOBUSINT_N (1ULL << IOBUSINT_N)
#define MASK_ESP_READ_N (1 << ADJUST(ESP_READ_N))
#define MASK64_ESP_READ_N (1ULL << ESP_READ_N)



static volatile bool trigger_trs_io_action = false;

#define IO_CORE1_ENABLE_INTR BIT0
#define IO_CORE1_DISABLE_INTR BIT1

static volatile bool io_task_started = false;
static volatile uint8_t intr_event = 0;
static volatile bool intr_enabled = true;



static volatile int16_t printer_data = -1;


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


static uint8_t spi_current_dir = 0xff;
static uint8_t spi_current_data = 0x00;

static inline void spi_write_data(uint8_t data)
{
  if (spi_current_dir == 0x00 && spi_current_data == data) {
    // The data to be written is already is already registered in the port
    // expander (via a previous write). Nothing to do.
    return;
  }
  if (!intr_enabled) {
    portENABLE_INTERRUPTS();
  }
  if (spi_current_dir != 0x00) {
    // Reconfigure to output
    spi_current_dir = 0x00;
    writePortExpander(MCP23S08_IODIR, spi_current_dir);
  }
  spi_current_data = data;
  writePortExpander(MCP23S08_GPIO, data);
  if (!intr_enabled) {
    portDISABLE_INTERRUPTS();
  }
}

static inline uint8_t spi_read_data()
{
  if (!intr_enabled) {
    portENABLE_INTERRUPTS();
  }
  if (spi_current_dir != 0xff) {
    // Reconfigure to input
    spi_current_dir = 0x00;
    writePortExpander(MCP23S08_IODIR, spi_current_dir);
  }
  uint8_t d = readPortExpander(MCP23S08_GPIO);
  if (!intr_enabled) {
    portDISABLE_INTERRUPTS();
  }
  return d;
}

static inline void trs_io_read() {
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
  uint8_t port = GPIO.in1.data & 0x0f;
  if (!trigger_trs_io_action) {
    uint8_t data = spi_read_data();
    if (port != 0x0f) {
      // This data was written to the printer port
      printer_data = data;
      trigger_trs_io_action = true;
    } else if (!TrsIO::outZ80(data)) {
      trigger_trs_io_action = true;
    }
  }
}

static inline void trs_io_write() {
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
  uint8_t port = GPIO.in1.data & 0x0f;
  uint32_t d;
  if (port == 0x0f) {
    d = trigger_trs_io_action ? 0xff : TrsIO::inZ80();
  } else {
    d = trigger_trs_io_action ? 0xff : trs_printer_read();
  }
  spi_write_data(d);
}

static inline void frehd_read() {
  uint8_t port = GPIO.in1.data & 0x0f;
  uint8_t data = spi_read_data();
  frehd_out(port, data);
}

static inline void frehd_write() {
  uint8_t port = GPIO.in1.data & 0x0f;
  spi_write_data(frehd_in(port));
}


static void io_task(void* p)
{
  io_task_started = true;
  portDISABLE_INTERRUPTS();
  intr_enabled = false;
  
  while(true) {
    // Wait for access the GAL to trigger this ESP
    while ((GPIO.in & MASK_ESP_SEL_N) && (intr_event == 0)) ;

    if (intr_event != 0) {
      if (intr_event & IO_CORE1_ENABLE_INTR) {
        portENABLE_INTERRUPTS();
        intr_event &= ~IO_CORE1_ENABLE_INTR;
        intr_enabled = true;
      } else if (intr_event & IO_CORE1_DISABLE_INTR) {
        portDISABLE_INTERRUPTS();
        intr_event &= ~IO_CORE1_DISABLE_INTR;
        intr_enabled = false;
      }
      continue;
    }

    const uint8_t s = (GPIO.in1.data & MASK_ESP_SEL_TRS_IO) ? 2 : 3;
    if (GPIO.in1.data & MASK_ESP_READ_N) {
      // Read data
      switch (s) {
      case 1:
        assert(0);
        break;
      case 2:
        trs_io_read();
        break;
      case 3:
        frehd_read();
        break;
      }
    } else {
      // Write data
      switch (s) {
      case 1:
        assert(0); // Shouldn't happen
        break;
      case 2:
        trs_io_write();
        break;
      case 3:
        frehd_write();
        break;
      }
    }
    
    // Release ESP_WAIT_RELEASE_N
    GPIO.out_w1ts = MASK_ESP_WAIT_RELEASE_N;

    // Wait for ESP_SEL_N to be de-asserted
    while (!(GPIO.in & MASK_ESP_SEL_N)) ;

    // Set ESP_WAIT_RELEASE_N to 0 for next IO command
    GPIO.out_w1tc = MASK_ESP_WAIT_RELEASE_N;
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
        REG_WRITE(GPIO_OUT_W1TS_REG, MASK_IOBUSINT_N);
      } else {
        trs_printer_write(printer_data);
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

  // GPIO pins 32-35 are use for A0-A3
  gpioConfig.pin_bit_mask = GPIO_SEL_32 | GPIO_SEL_33 | GPIO_SEL_34 | GPIO_SEL_35;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);

  // Configure ESP_READ_N
  gpioConfig.pin_bit_mask = MASK64_ESP_READ_N;
  gpio_config(&gpioConfig);
  
  // Configure ESP_SEL_N
  gpioConfig.pin_bit_mask = MASK64_ESP_SEL_N;
  gpio_config(&gpioConfig);

  // Configure ESP_SEL_TRS_IO
  gpioConfig.pin_bit_mask = MASK64_ESP_SEL_TRS_IO;
  gpio_config(&gpioConfig);

  // Configure IOBUSINT_N & ESP_WAIT_RELEASE_N
  gpioConfig.pin_bit_mask = MASK64_IOBUSINT_N | MASK64_ESP_WAIT_RELEASE_N;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
  
  // Set IOBUSINT_N to 0
  gpio_set_level((gpio_num_t) IOBUSINT_N, 0);
  
  // Set ESP_WAIT_RELEASE_N to 0
  gpio_set_level((gpio_num_t) ESP_WAIT_RELEASE_N, 0);

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, tskIDLE_PRIORITY + 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(action_task, "action", 6000, NULL, 1, NULL, 0);
}
