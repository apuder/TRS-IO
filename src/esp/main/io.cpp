
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


// GPIO pins 12-19
#define GPIO_DATA_BUS_MASK 0b11111111000000000000

#ifdef CONFIG_TRS_IO_MODEL_1
#define ESP_S0 34
#define ESP_S1 35
#define IOBUSINT_N 33
#else
// GPIO(23): ESP_SEL_N (TRS-IO or Frehd)
#define ESP_SEL_TRS_IO 39
#define IOBUSINT_N 25
#endif

#define ESP_SEL_N 23
#define ESP_READ_N 36
#define ESP_WAIT_RELEASE_N 27

#define ADJUST(x) ((x) < 32 ? (x) : (x) - 32)

#define MASK_ESP_SEL_N (1 << ADJUST(ESP_SEL_N))
#define MASK64_ESP_SEL_N (1ULL << ESP_SEL_N)
#ifdef CONFIG_TRS_IO_MODEL_1
#define MASK_ESP_S0 (1 << ADJUST(ESP_S0))
#define MASK64_ESP_S0 (1ULL << ESP_S0)
#define MASK_ESP_S1 (1 << ADJUST(ESP_S1))
#define MASK64_ESP_S1 (1ULL << ESP_S1)
#else
#define MASK_ESP_SEL_TRS_IO (1 << ADJUST(ESP_SEL_TRS_IO))
#define MASK64_ESP_SEL_TRS_IO (1ULL << ESP_SEL_TRS_IO)
#endif
#define MASK_ESP_WAIT_RELEASE_N (1 << ADJUST(ESP_WAIT_RELEASE_N))
#define MASK64_ESP_WAIT_RELEASE_N (1ULL << ESP_WAIT_RELEASE_N)
#define MASK_IOBUSINT_N (1 << ADJUST(IOBUSINT_N))
#define MASK64_IOBUSINT_N (1ULL << IOBUSINT_N)
#define MASK_ESP_READ_N (1 << ADJUST(ESP_READ_N))
#define MASK64_ESP_READ_N (1ULL << ESP_READ_N)

#define GPIO_OUTPUT_DISABLE(mask) GPIO.enable_w1tc = (mask)

#define GPIO_OUTPUT_ENABLE(mask) GPIO.enable_w1ts = (mask)

// loader_frehd.bin
extern const uint8_t loader_frehd_start[] asm("_binary_loader_frehd_bin_start");
extern const uint8_t loader_frehd_end[] asm("_binary_loader_frehd_bin_end");

static volatile bool trigger_trs_io_action = false;

#define IO_CORE1_ENABLE_INTR BIT0
#define IO_CORE1_DISABLE_INTR BIT1

static volatile bool io_task_started = false;
static volatile uint8_t intr_event = 0;
static volatile bool intr_enabled = true;


// Active low
#define TRS_IO_DATA_READY_BIT 0x20
// Active high
#define TRS_IO_HEARTBEAT_BIT 0x80

static volatile uint8_t fdc_37e0 = TRS_IO_DATA_READY_BIT;
static volatile bool heartbeat_triggered = false;

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

#ifdef CONFIG_TRS_IO_MODEL_1
static void timer25ms(TimerHandle_t pxTimer)
{
  heartbeat_triggered = true;
  REG_WRITE(GPIO_OUT1_W1TS_REG, MASK_IOBUSINT_N);
}

static inline uint8_t read_a0_a7()
{
  if (!intr_enabled) {
    portENABLE_INTERRUPTS();
  }
  uint8_t lsb = readPortExpander(MCP23S17, MCP23S17_GPIOA);
  if (!intr_enabled) {
    portDISABLE_INTERRUPTS();
  }
  return lsb;
}

static inline uint16_t read_a0_a15()
{
  if (!intr_enabled) {
    portENABLE_INTERRUPTS();
  }
  uint8_t lsb = readPortExpander(MCP23S17, MCP23S17_GPIOA);
  uint8_t msb = readPortExpander(MCP23S17, MCP23S17_GPIOB);
  if (!intr_enabled) {
    portDISABLE_INTERRUPTS();
  }
  uint16_t addr = lsb | (msb << 8);
  return addr;
}
#endif

static inline void trs_io_read() {
#ifdef CONFIG_TRS_IO_MODEL_1
  fdc_37e0 |= TRS_IO_DATA_READY_BIT;
#else
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
#endif
  if (!trigger_trs_io_action) {
    uint8_t data = GPIO.in >> 12;
    if (!TrsIO::outZ80(data)) {
      trigger_trs_io_action = true;
    }
  }
}

static inline void trs_io_write() {
#ifdef CONFIG_TRS_IO_MODEL_1
  fdc_37e0 |= TRS_IO_DATA_READY_BIT;
#else
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
#endif
  GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
  uint32_t d = trigger_trs_io_action ? 0xff : TrsIO::inZ80();
  d <<= 12;
  REG_WRITE(GPIO_OUT_W1TS_REG, d);
  d = d ^ GPIO_DATA_BUS_MASK;
  REG_WRITE(GPIO_OUT_W1TC_REG, d);
}

static inline void frehd_read() {
#ifdef CONFIG_TRS_IO_MODEL_1
  uint8_t port = read_a0_a7();
#else
  uint8_t port = GPIO.in1.data & 0x0f;
#endif
  uint8_t data = GPIO.in >> 12;
  frehd_out(port, data);
}

static inline void frehd_write() {
#ifdef CONFIG_TRS_IO_MODEL_1
  uint8_t port = read_a0_a7();
#else
  uint8_t port = GPIO.in1.data & 0x0f;
#endif
  uint32_t d = frehd_in(port) << 12;

  GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
  REG_WRITE(GPIO_OUT_W1TS_REG, d);
  d = d ^ GPIO_DATA_BUS_MASK;
  REG_WRITE(GPIO_OUT_W1TC_REG, d);
}

#ifdef CONFIG_TRS_IO_MODEL_1
static inline void floppy_write()
{
  static uint8_t i = 0;
  static const uint8_t len = loader_frehd_end - loader_frehd_start;

  uint32_t d = 0xff;
  uint16_t addr = read_a0_a15();
  
  switch(addr) {
  case 0x37e0:
    d = fdc_37e0;
    if (heartbeat_triggered) {
      d |= TRS_IO_HEARTBEAT_BIT;
      heartbeat_triggered = false;
    }
    REG_WRITE(GPIO_OUT1_W1TC_REG, MASK_IOBUSINT_N);
    break;
  case 0x37ec:
    d = 2;
    break;
  case 0x37ef:
    if (i < len) {
      d = loader_frehd_start[i];
    }
    i++;
    break;
  }

  d = d << 12;

  GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
  REG_WRITE(GPIO_OUT_W1TS_REG, d);
  d = d ^ GPIO_DATA_BUS_MASK;
  REG_WRITE(GPIO_OUT_W1TC_REG, d);
}
#endif

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

#ifdef CONFIG_TRS_IO_MODEL_1
    // Read pins S0 and S1
    const uint8_t s = (GPIO.in1.data >> 2) & 3;
#else
    const uint8_t s = (GPIO.in1.data & MASK_ESP_SEL_TRS_IO) ? 2 : 3;
#endif
    if (GPIO.in1.data & MASK_ESP_READ_N) {
      // Read data
#ifdef CONFIG_TRS_IO_USE_RETROSTORE_PCB
      trs_io_read();
#else
      switch (s) {
      case 1:
	assert(0); // Shouldn't happen
	break;
      case 2:
	trs_io_read();
	break;
      case 3:
	frehd_read();
	break;
      }
#endif
    } else {
      // Write data
#ifdef CONFIG_TRS_IO_USE_RETROSTORE_PCB
      trs_io_write();
#else
      switch (s) {
      case 1:
#ifdef CONFIG_TRS_IO_MODEL_1
	floppy_write();
#else
	assert(0); // Shouldn't happen
#endif
	break;
      case 2:
	trs_io_write();
	break;
      case 3:
	frehd_write();
	break;
      }
#endif
    }
    
    // Release ESP_WAIT_RELEASE_N
    GPIO.out_w1ts = MASK_ESP_WAIT_RELEASE_N;

    // Wait for ESP_SEL_N to be de-asserted
    while (!(GPIO.in & MASK_ESP_SEL_N)) ;
    
    // Set ESP_WAIT_RELEASE_N to 0 for next IO command
    GPIO.out_w1tc = MASK_ESP_WAIT_RELEASE_N;

    GPIO_OUTPUT_DISABLE(GPIO_DATA_BUS_MASK);
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
      TrsIO::processInBackground();
      trigger_trs_io_action = false;
#ifdef CONFIG_TRS_IO_MODEL_1
      fdc_37e0 &= ~TRS_IO_DATA_READY_BIT;
#else
      REG_WRITE(GPIO_OUT_W1TS_REG, MASK_IOBUSINT_N);
#endif
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

#ifndef CONFIG_TRS_IO_MODEL_1
  // GPIO pins 32-35 are use for A0-A3
  gpioConfig.pin_bit_mask = GPIO_SEL_32 | GPIO_SEL_33 | GPIO_SEL_34 |
    GPIO_SEL_35;
  gpio_config(&gpioConfig);
#endif

  // Configure ESP_READ_N
  gpioConfig.pin_bit_mask = MASK64_ESP_READ_N;
  gpio_config(&gpioConfig);
  
  // Configure ESP_SEL_N
  gpioConfig.pin_bit_mask = MASK64_ESP_SEL_N;
  gpio_config(&gpioConfig);

#ifdef CONFIG_TRS_IO_MODEL_1
  // Configure ESP_S0 and ESP_S1
  gpioConfig.pin_bit_mask = MASK64_ESP_S0 | MASK64_ESP_S1;
  gpio_config(&gpioConfig);
#else
  // Configure ESP_SEL_TRS_IO
  gpioConfig.pin_bit_mask = MASK64_ESP_SEL_TRS_IO;
  gpio_config(&gpioConfig);
#endif

  // Configure IOBUSINT_N & ESP_WAIT_RELEASE_N
  gpioConfig.pin_bit_mask = MASK64_IOBUSINT_N | MASK64_ESP_WAIT_RELEASE_N;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
  
  // Set IOBUSINT_N to 0
  gpio_set_level((gpio_num_t) IOBUSINT_N, 0);
  
  // Set ESP_WAIT_RELEASE_N to 0
  gpio_set_level((gpio_num_t) ESP_WAIT_RELEASE_N, 0);

#ifdef CONFIG_TRS_IO_MODEL_1
  init_spi();
  TimerHandle_t timer = xTimerCreate("Heartbeat", 25, pdTRUE, NULL, timer25ms);
  assert(xTimerStart(timer, 0) == pdPASS);
#endif

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, tskIDLE_PRIORITY + 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(action_task, "action", 6000, NULL, 1, NULL, 0);
}
