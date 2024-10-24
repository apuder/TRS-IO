
#include "trs-io.h"
#include "frehd.h"
#include "io.h"
#include "button.h"
#include "led.h"
#include "settings.h"
#include "spi.h"
#include "wifi.h"
#include "http.h"
#include "ptrs.h"
#include "event.h"
#include "trs-fs.h"
#include "esp_task.h"
#include "esp_event.h"
#include "tcpip.h"
#include "retrostore.h"
#include "retrostore-defs.h"
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "web_debugger.h"
#include "keyb.h"
#include "jtag.h"
#include "spi.h"
#include "rst.h"

#define ESP_REQ GPIO_NUM_34
#ifdef CONFIG_TRS_IO_PP
#define ESP_DONE GPIO_NUM_18
#define SD_CARD GPIO_NUM_4
#else
#define ESP_DONE GPIO_NUM_27
#define ESP_FULL_ADDR GPIO_NUM_33
#endif


#define ADJUST(x) ((x) < 32 ? (x) : (x) - 32)

#define MASK_ESP_REQ (1 << ADJUST(ESP_REQ))
#define MASK64_ESP_REQ (1ULL << ESP_REQ)
#define MASK_ESP_DONE (1 << ADJUST(ESP_DONE))
#define MASK64_ESP_DONE (1ULL << ESP_DONE)
#ifndef CONFIG_TRS_IO_PP
#define MASK_ESP_FULL_ADDR (1 << ADJUST(ESP_FULL_ADDR))
#define MASK64_ESP_FULL_ADDR (1ULL << ESP_FULL_ADDR)
#endif


static volatile bool DRAM_ATTR trigger_trs_io_action = false;


#define IO_CORE1_ENABLE_INTR BIT0
#define IO_CORE1_DISABLE_INTR BIT1
#define TRS_IO_DONE BIT2

static volatile bool io_task_started = false;
static volatile uint8_t DRAM_ATTR intr_event = 0;


#ifndef CONFIG_TRS_IO_MODEL_3
/***********************************************************************************
 * XRay
 ***********************************************************************************/

#include "retrostore.h"

extern retrostore::RsSystemState trs_state;
extern int trs_state_token;

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
#ifndef CONFIG_TRS_IO_PP
  bool full_addr = !gpio_get_level(ESP_FULL_ADDR);
#else
  bool full_addr = false;
#endif

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
#endif

//-----------------------------------------------------------------

static inline uint8_t dbus_read()
{
  return spi_dbus_read();
}

static inline void dbus_write(uint8_t d)
{
  spi_dbus_write(d);
}

static inline uint8_t abus_read()
{
  return spi_abus_read();
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

static inline void printer_read() {
  uint8_t d = trs_printer_read();
  dbus_write(d);
}

static inline void printer_write() {
  uint8_t data = dbus_read();
  trs_printer_write(data);
}

static volatile bool DRAM_ATTR esp_req_triggered = false;
static volatile uint8_t DRAM_ATTR sd_card_eject_countdown = 0;

static void IRAM_ATTR esp_req_isr_handler(void* arg)
{
  esp_req_triggered = true;
}

#ifdef CONFIG_TRS_IO_PP
static void IRAM_ATTR sd_card_eject_isr_handler(void* arg)
{
  sd_card_eject_countdown = 0xff;
}
#endif

static void IRAM_ATTR io_task(void* p)
{
#ifndef CONFIG_TRS_IO_MODEL_3
  init_xray();
#endif

  evt_signal(EVT_ESP_READY);

  while(true) {
    while (!esp_req_triggered) ;
    esp_req_triggered = false;
#ifndef CONFIG_TRS_IO_PP
    // Read pins [S2..S0,A3..A0]
    const uint8_t s = GPIO.in >> 12;
    const uint8_t mask = 0x70;
#else
    // Read pins [S3..S0] to upper nibble
    const uint8_t s = GPIO.in >> 8;
    const uint8_t mask = 0xf0;
#endif
    switch(s & mask) {
    case 0x00:
      trs_io_write();
      break;
    case 0x10:
      trs_io_read();
      break;
    case 0x20:
#if defined(CONFIG_TRS_IO_MODEL_3) || defined(CONFIG_TRS_IO_PP)
      frehd_write(spi_abus_read());
#else
      frehd_write(s);
#endif
      break;
    case 0x30:
#if defined(CONFIG_TRS_IO_MODEL_3) || defined(CONFIG_TRS_IO_PP)
      frehd_read(spi_abus_read());
#else
      frehd_read(s);
#endif
      break;
    case 0x40:
      printer_read();
      break;
    case 0x50:
      printer_write();
      break;
#ifndef CONFIG_TRS_IO_MODEL_3
    case 0x60:
      xray_status = XRAY_STATUS_BREAKPOINT;
      break;
#endif
#ifdef CONFIG_TRS_IO_PP
    case 0xd0:
      evt_signal(EVT_CASS_MOTOR_ON);
      break;
    case 0xe0:
      evt_signal(EVT_CASS_MOTOR_OFF);
      break;
    case 0xf0:
      evt_send_esp_status();
      break;
#endif
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
  is_status_button_short_press();
  is_status_button_long_press();
#ifdef CONFIG_TRS_IO_PP
  is_reset_button_short_press();
  is_reset_button_long_press();
#endif

  while (true) {
#ifdef CONFIG_TRS_IO_PP
    if (sd_card_eject_countdown != 0) {
      sd_card_eject_countdown--;
      if (sd_card_eject_countdown == 1) {
        // gpio_get_level(SD_CARD) == true -> SD card ejected
        init_trs_fs_posix();
        evt_signal((get_posix_err_msg() == NULL) ? EVT_SD_MOUNTED : EVT_SD_UNMOUNTED);
        ESP_LOGI("ACTION", "SD card %s", get_posix_err_msg() == NULL ? "inserted" : "ejected");
      }
    }

    check_keyb();
#endif

    frehd_check_action();

    check_events();

    if (trigger_trs_io_action) {
      bool deferDone = TrsIO::processInBackground();
      trigger_trs_io_action = false;
      if (!deferDone) {
        spi_trs_io_done();
      }
    }

#ifndef CONFIG_TRS_IO_MODEL_3
    if ((trs_state.regions.size() != 0) && (xray_status != XRAY_STATUS_RUN)) {
      // Load memory regions
      for (int i = 0; i < trs_state.regions.size(); i++) {
        retrostore::RsMemoryRegion* region = &trs_state.regions[i];
        if (region->start == 0x3c00) {
          // Ignore screenshot
          continue;
        }
        int start = region->start;
        int left = region->length;
        const int fragment_size = 4096;
        do {
          rs.DownloadStateMemoryRange(trs_state_token, start, fragment_size, region);
          uint8_t* buf = region->data.get();
          assert(region->start == start);
          assert(region->length <= fragment_size);
          for (int j = 0; j < region->length; j++) {
            spi_bram_poke(start + j, *buf++);
          }
          start += region->length;
          left -= region->length;
        } while (left > 0);
      }
      trs_state.regions.clear();
      spi_clear_breakpoint(0);
      spi_xray_resume();
      xray_status = XRAY_STATUS_RUN;
    }
#endif

    if (is_status_button_long_press()) {
      settings_reset_all();

      reboot_trs_io();
    }

#ifdef CONFIG_TRS_IO_PP
    if (is_reset_button_short_press()) {
      spi_ptrs_rst();
    }
    if (is_reset_button_long_press()) {
      ptrs_load_rom();
    }
#endif

    if (is_status_button_short_press()) {
      evt_send_esp_status();

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

#if defined(CONFIG_TRS_IO_MODEL_1)
// GPIO pins 12-18 (7 pins) are used for S0-S2 and A0-A4
  gpioConfig.pin_bit_mask = GPIO_SEL_12 | GPIO_SEL_13 | GPIO_SEL_14 |
    GPIO_SEL_15 | GPIO_SEL_16 |
    GPIO_SEL_17 | GPIO_SEL_18;
#elif defined(CONFIG_TRS_IO_PP)
  // GPIO pins 12-15 (4 pins) are used for S0-S3
  gpioConfig.pin_bit_mask = GPIO_SEL_12 | GPIO_SEL_13 | GPIO_SEL_14 | GPIO_SEL_15;
#else
  // GPIO pins 16-18 (3 pins) are used for S0-S2
  gpioConfig.pin_bit_mask = GPIO_SEL_16 | GPIO_SEL_17 | GPIO_SEL_18;
#endif
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);

  // Configure ESP_SEL_N
  gpioConfig.pin_bit_mask = MASK64_ESP_REQ;
  gpioConfig.intr_type = GPIO_INTR_POSEDGE;
  gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE; // enable pulldown
  gpio_config(&gpioConfig);
  //gpio_install_isr_service(0);
  gpio_isr_handler_add(ESP_REQ, esp_req_isr_handler, NULL);
  gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE; // restore disable

#ifdef CONFIG_TRS_IO_PP
  // Configure SD Card eject trigger
  gpioConfig.pin_bit_mask = GPIO_SEL_4;
  gpioConfig.intr_type = GPIO_INTR_ANYEDGE;
  gpio_config(&gpioConfig);
  gpio_isr_handler_add(SD_CARD, sd_card_eject_isr_handler, NULL);
#endif

  // Configure ESP_DONE
  gpioConfig.pin_bit_mask = MASK64_ESP_DONE;
  gpioConfig.mode = GPIO_MODE_OUTPUT;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
  
  // Set ESP_DONE to 0
  gpio_set_level((gpio_num_t) ESP_DONE, 0);

#if !defined(CONFIG_TRS_IO_MODEL_3) && !defined(CONFIG_TRS_IO_PP)
  // Configure ESP_FULL_ADDR
  gpioConfig.pin_bit_mask = MASK64_ESP_FULL_ADDR;
  gpioConfig.mode = GPIO_MODE_INPUT;
  gpioConfig.pull_up_en = GPIO_PULLUP_ENABLE;
  gpioConfig.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&gpioConfig);
#endif

  xTaskCreatePinnedToCore(io_task, "io", 6000, NULL, tskIDLE_PRIORITY + 2,
                          NULL, 1);
  xTaskCreatePinnedToCore(action_task, "action", 6000, NULL, 1, NULL, 0);
}
