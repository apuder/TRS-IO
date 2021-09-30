
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
#define ESP_SEL_N 27
#define ESP_WAIT_RELEASE_N 2
#else
// GPIO(23): ESP_SEL_N (TRS-IO or Frehd)
#define ESP_SEL_TRS_IO 39
#define IOBUSINT_N 25
#define ESP_SEL_N 23
#define ESP_WAIT_RELEASE_N 27
#endif

#define ESP_READ_N 36

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

static volatile int16_t printer_data = -1;

#ifdef CONFIG_TRS_IO_ENABLE_XRAY
static uint8_t vram[32 * 1024];
#endif

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
  uint8_t port = read_a0_a7() & 0x0f;
#else
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
  uint8_t port = GPIO.in1.data & 0x0f;
#endif
  if (!trigger_trs_io_action) {
    uint8_t data = GPIO.in >> 12;
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
#ifdef CONFIG_TRS_IO_MODEL_1
  fdc_37e0 |= TRS_IO_DATA_READY_BIT;
  uint8_t port = read_a0_a7() & 0x0f;
#else
  REG_WRITE(GPIO_OUT_W1TC_REG, MASK_IOBUSINT_N);
  uint8_t port = GPIO.in1.data & 0x0f;
#endif
  GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
  uint32_t d;
  if (port == 0x0f) {
    d = trigger_trs_io_action ? 0xff : TrsIO::inZ80();
  } else {
    d = trigger_trs_io_action ? 0xff : trs_printer_read();
  }
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

/***********************************************************************************
 * XRay
 ***********************************************************************************/

static const unsigned char fill_bin[] = {
  0x21, 0x00, 0x3c, 0x01, 0x00, 0x04, 0x36, 0xbf, 0x23, 0x0b, 0x78, 0xb1,
  0x20, 0xf8, 0xc9
};

static const unsigned char xray_stub_bin[] = {
  0xed, 0x73, 0xd0, 0x42, 0xe5, 0x6c, 0x26, 0xff, 0x6e, 0xe1, 0xe5, 0x26,
  0xff, 0x6e, 0x6a, 0x6e, 0x6b, 0x6e, 0x68, 0x6e, 0x69, 0x6e, 0xc5, 0xed,
  0x4b, 0xd0, 0x42, 0x68, 0x6e, 0x69, 0x6e, 0xf5, 0xc1, 0x68, 0x6e, 0x69,
  0x6e, 0xc1, 0xe1, 0x18, 0xd7
};

typedef struct {
  #if 0
  uint16_t af_p;
  uint16_t bc_p;
  uint16_t de_p;
  uint16_t hl_p;
  uint16_t ix;
  uint16_t iy;
  #endif
  uint16_t af;
  uint16_t sp;
  uint16_t bc;
  uint16_t de;
  uint16_t hl;
} XRAY_Z80_REGS;

static union {
  XRAY_Z80_REGS xray_z80_regs;
  uint8_t vram[sizeof(XRAY_Z80_REGS)];
} xray_vram_alt;


#define XRAY_STATUS_RUN 0
#define XRAY_STATUS_CONTINUE 1
#define XRAY_STATUS_BREAKPOINT 2

static volatile uint8_t xray_status = XRAY_STATUS_RUN;
static volatile bool trigger_xray_action = false;

#define XRAY_MAX_BREAKPOINTS 5

static uint16_t xray_breakpoints[XRAY_MAX_BREAKPOINTS];
static uint8_t xray_active_breakpoints[XRAY_MAX_BREAKPOINTS];
static uint16_t xray_breakpoint_pc;
static uint8_t xray_breakpoint_idx;


static void init_xray()
{
  memset(xray_active_breakpoints, 0, sizeof(xray_active_breakpoints));
  memset(vram, 0, sizeof(vram));
  memcpy(vram, fill_bin, sizeof(fill_bin));
}

static void xray_set_breakpoint(uint8_t idx, uint16_t addr)
{
  xray_breakpoints[idx] = addr;
  xray_active_breakpoints[idx] = 1;
}

static void xray_clean_breakpoint(uint8_t idx)
{
  xray_active_breakpoints[idx] = 0;
}

static void xray_continue()
{
  trigger_xray_action = false;
  xray_status = XRAY_STATUS_CONTINUE;
}

#ifdef CONFIG_TRS_IO_MODEL_1
static inline void ram_read()
{
  uint16_t addr = read_a0_a15();
  uint8_t data = GPIO.in >> 12;

#ifdef CONFIG_TRS_IO_ENABLE_XRAY
  if (addr & 0x8000) {
    vram[addr & 0x7fff] = data;
    return;
  }
#endif

  switch(addr) {
  case 0x37e0:
    //printf("%04X = %02X\n", addr, data);
    break;
  case 0x37ec:
    //printf("%04X = %02X\n", addr, data);
    break;
  case 0x37ef:
    //printf("%04X = %02X\n", addr, data);
    break;
  }
}


static inline void ram_write()
{
  static uint8_t i = 0;
  static const uint8_t len = loader_frehd_end - loader_frehd_start;

  uint32_t d = 0xff;
  uint16_t addr = read_a0_a15();
  
#ifdef CONFIG_TRS_IO_ENABLE_XRAY
  if (addr & 0x8000) {
    static bool second_time = false;
    static uint8_t* sp;

    if (xray_status == XRAY_STATUS_RUN) {
      for (int i = 0; i < XRAY_MAX_BREAKPOINTS; i++) {
        if (xray_active_breakpoints[i] && xray_breakpoints[i] == addr) {
          xray_status = XRAY_STATUS_BREAKPOINT;
          xray_breakpoint_pc = addr;
          xray_breakpoint_idx = i;
          break;
        }
      }
      d = vram[addr & 0x7fff];
    }

    if (xray_status == XRAY_STATUS_BREAKPOINT || xray_status == XRAY_STATUS_CONTINUE) {
      uint16_t stub_idx = addr - xray_breakpoint_pc;
      if (stub_idx == 0) {
        sp = &xray_vram_alt.vram[sizeof(XRAY_Z80_REGS) - 1];
        if (second_time) {
          trigger_xray_action = true;
        }
        second_time = true;
      }
      if (stub_idx == 0 && xray_status == XRAY_STATUS_CONTINUE) {
        d = vram[addr & 0x7fff];
        xray_status = XRAY_STATUS_RUN;
        second_time = false;
      } else {
        if(stub_idx < sizeof(xray_stub_bin)) {
          d = xray_stub_bin[stub_idx];
        } else {
          // Access must be loading pf SP at the end of the debug stub
          assert((addr & 0xff00) == 0xff00);
          *sp-- = addr & 0xff;
          d = 0;
        }
      }
    }
  } else {
#endif

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

#ifdef CONFIG_TRS_IO_ENABLE_XRAY
  }
#endif

  d = d << 12;

  GPIO_OUTPUT_ENABLE(GPIO_DATA_BUS_MASK);
  REG_WRITE(GPIO_OUT_W1TS_REG, d);
  d = d ^ GPIO_DATA_BUS_MASK;
  REG_WRITE(GPIO_OUT_W1TC_REG, d);
}
#endif

static void io_task(void* p)
{
#ifdef CONFIG_TRS_IO_ENABLE_XRAY
  init_xray();
#endif

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
#ifdef CONFIG_TRS_IO_MODEL_1
        ram_read();
#else
        assert(0);
#endif
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
        ram_write();
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

    if (trigger_xray_action) {
      // Breakpoint triggered
    }

    if (trigger_trs_io_action) {
      if (printer_data == -1) {
        TrsIO::processInBackground();
        trigger_trs_io_action = false;
#ifdef CONFIG_TRS_IO_MODEL_1
        fdc_37e0 &= ~TRS_IO_DATA_READY_BIT;
#else
        REG_WRITE(GPIO_OUT_W1TS_REG, MASK_IOBUSINT_N);
#endif
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
  TimerHandle_t timer = xTimerCreate("Heartbeat", 25, pdTRUE, NULL, timer25ms);
  assert(xTimerStart(timer, 0) == pdPASS);
#endif

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, tskIDLE_PRIORITY + 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(action_task, "action", 6000, NULL, 1, NULL, 0);
}
