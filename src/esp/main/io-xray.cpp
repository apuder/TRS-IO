
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
#include "web_debugger.h"
#include "spi.h"

#define ESP_REQ GPIO_NUM_34
#define ESP_DONE GPIO_NUM_27
#define ESP_FULL_ADDR GPIO_NUM_33


#define ADJUST(x) ((x) < 32 ? (x) : (x) - 32)

#define MASK_ESP_REQ (1 << ADJUST(ESP_REQ))
#define MASK64_ESP_REQ (1ULL << ESP_REQ)
#define MASK_ESP_DONE (1 << ADJUST(ESP_DONE))
#define MASK64_ESP_DONE (1ULL << ESP_DONE)
#define MASK_ESP_FULL_ADDR (1 << ADJUST(ESP_FULL_ADDR))
#define MASK64_ESP_FULL_ADDR (1ULL << ESP_FULL_ADDR)


static volatile bool DRAM_ATTR trigger_trs_io_action = false;


#define IO_CORE1_ENABLE_INTR BIT0
#define IO_CORE1_DISABLE_INTR BIT1
#define TRS_IO_DONE BIT2

static volatile bool io_task_started = false;
static volatile uint8_t DRAM_ATTR intr_event = 0;
static volatile bool DRAM_ATTR intr_enabled = true;


/***********************************************************************************
 * XRay
 ***********************************************************************************/

uint8_t* xray_upper_ram;

#define XRAY_PC_OFFSET 11

extern const uint8_t xray_stub_start[] asm("_binary_xray_stub_bin_start");
extern const uint8_t xray_stub_end[] asm("_binary_xray_stub_bin_end");
static const uint8_t xray_stub_len = xray_stub_end - xray_stub_start;


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

static void setup_xram_stub() {
  while(spi_get_cookie() != FPGA_COOKIE) {
    vTaskDelay(10);
  }
  for (int i = 0; i < xray_stub_len; i++) {
    spi_xram_poke_code(i, xray_stub_start[i]);
  }
  spi_xram_poke_data(0, 0xc9); // 0xc9 == Z80 RET
}

static char* on_trx_get_resource(TRX_RESOURCE_TYPE type) {
  return NULL;
}

void on_trx_control_callback(TRX_CONTROL_TYPE type) {
  if (type == TRX_CONTROL_TYPE_STEP ||
      type == TRX_CONTROL_TYPE_CONTINUE) {
    spi_xray_resume();
    xray_status = XRAY_STATUS_RUN;
  }
}

void on_trx_get_state_update(TRX_SystemState* state) {
  printf("on_trx_get_state_update\n");
  state->paused = xray_status != XRAY_STATUS_RUN;
  if (state->paused) {
    printf("Hit breakpoint\n");
  }
    for (int i = 0; i < sizeof(XRAY_Z80_REGS); i++) {
      xray_vram_alt.vram[sizeof(XRAY_Z80_REGS) - 1 - i] = spi_xram_peek_data(255 - i);
    }

    state->registers.pc = xray_vram_alt.xray_z80_regs.pc - XRAY_PC_OFFSET;
    state->registers.sp = xray_vram_alt.xray_z80_regs.sp;
    state->registers.af = xray_vram_alt.xray_z80_regs.af;
    state->registers.bc = xray_vram_alt.xray_z80_regs.bc;
    state->registers.de = xray_vram_alt.xray_z80_regs.de;
    state->registers.hl = xray_vram_alt.xray_z80_regs.hl;

    state->registers.af_prime = xray_vram_alt.xray_z80_regs.af_p;
    state->registers.bc_prime = xray_vram_alt.xray_z80_regs.bc_p;
    state->registers.de_prime = xray_vram_alt.xray_z80_regs.de_p;
    state->registers.hl_prime = xray_vram_alt.xray_z80_regs.hl_p;
    state->registers.ix = xray_vram_alt.xray_z80_regs.ix;
    state->registers.iy = xray_vram_alt.xray_z80_regs.iy;

    // Not supported.
    state->registers.i = 0;
    state->registers.iff1 = 0;
    state->registers.iff2 = 0;
  //}
}

void on_trx_add_breakpoint(int bp_id, uint16_t addr, TRX_BREAK_TYPE type) {
  if (type != TRX_BREAK_PC) return;
  spi_set_breakpoint(bp_id, addr);
  printf("set breakpoint(%d): 0x%04x\n", bp_id, addr);
}

void on_trx_remove_breakpoint(int bp_id) {
  spi_clear_breakpoint(bp_id);
  printf("clear breakpoint(%d)\n", bp_id);
}

uint8_t trx_read_memory(uint16_t addr) {
//  return spi_bram_peek(addr);
  uint8_t d = spi_bram_peek(addr);
  printf("peek(%04x): %02x\n", addr, d);
  return d;
}

void trx_write_memory(uint16_t addr, uint8_t value) {
  spi_bram_poke(addr, value);
}

static void init_xray()
{
  bool full_addr = !gpio_get_level(ESP_FULL_ADDR);

  TRX_Context* ctx = get_default_trx_context();
  ctx->system_name = "Model 1";
  ctx->model = MODEL_I;
  ctx->capabilities.memory_range.start = full_addr ? 0 : 0x8000;
  ctx->capabilities.memory_range.length = 20;  // For demo only. fill program covered.
  ctx->capabilities.max_breakpoints = XRAY_MAX_BREAKPOINTS;
  ctx->capabilities.alt_single_step_mode = true;

  // TODO the rest....
  ctx->control_callback = &on_trx_control_callback;
  ctx->read_memory = &trx_read_memory;
  ctx->write_memory = &trx_write_memory;
  ctx->breakpoint_callback = &on_trx_add_breakpoint;
  ctx->remove_breakpoint_callback = &on_trx_remove_breakpoint;
  ctx->get_resource = &on_trx_get_resource;
  ctx->get_state_update = &on_trx_get_state_update;


  spi_set_full_addr(full_addr);
  init_trs_xray(ctx);
  setup_xram_stub();
}

//-----------------------------------------------------------------

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
  uint8_t d = frehd_in(port);
  dbus_write(d);
}

static inline void printer_write() {
  uint8_t data = spi_get_printer_byte();
  trs_printer_write(data);
}

static volatile bool DRAM_ATTR esp_req_triggered = false;

static void IRAM_ATTR esp_req_isr_handler(void* arg)
{
  esp_req_triggered = true;
}

static void IRAM_ATTR io_task(void* p)
{
  init_xray();

  while(true) {
    while (!esp_req_triggered) ;
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
    case 0x40:
      printer_write();
      break;
    case 0x50:
      xray_status = XRAY_STATUS_BREAKPOINT;
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
      TrsIO::processInBackground();
      trigger_trs_io_action = false;
      spi_trs_io_done();
    }

    if ((xray_upper_ram != NULL) && (xray_status != XRAY_STATUS_RUN)) {
      // Load upper 32K
      for(int i = 0; i < 32 * 1024; i++) {
        spi_bram_poke(0x8000 + i, xray_upper_ram[i]);
      }
      free(xray_upper_ram);
      xray_upper_ram = NULL;
      spi_clear_breakpoint(0);
      spi_xray_resume();
      xray_status = XRAY_STATUS_RUN;
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

  // Configure ESP_FULL_ADDR
  gpioConfig.pin_bit_mask = MASK64_ESP_FULL_ADDR;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, tskIDLE_PRIORITY + 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(action_task, "action", 6000, NULL, 1, NULL, 0);
}
