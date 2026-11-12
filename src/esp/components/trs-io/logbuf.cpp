// vim: shiftwidth=2
//
// Log ring buffer in PSRAM. See logbuf.h.
//
// Internal SRAM is scarce on this device, so all buffers live in PSRAM:
//   - the ring itself in .ext_ram_noinit (survives resets),
//   - the formatting scratch line in .ext_ram.bss.
// Only a few words of state (and a FreeRTOS mutex) use internal RAM.
//
// The ESP32's PSRAM is accessed through a write-back cache. Without a
// write-back before reset, the last log lines (and the panic dump) would
// still be sitting in the cache and be lost. We therefore wrap
// esp_restart_noos() at link time (see CMakeLists.txt), which every software
// restart path goes through: esp_restart(), the panic handler, the brownout
// handler.
//
// To capture crash dumps, the panic_print_* functions are wrapped as well:
// the panic handler prints registers and backtrace through them.

#include "logbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>

#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_private/cache_utils.h"
#include "esp_private/esp_psram_extram.h"
#include "esp_private/panic_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#if !CONFIG_SPIRAM_ALLOW_NOINIT_SEG_EXTERNAL_MEMORY
// Without it, EXT_RAM_NOINIT_ATTR silently falls back to internal RAM.
#error "logbuf needs CONFIG_SPIRAM_ALLOW_NOINIT_SEG_EXTERNAL_MEMORY=y"
#endif
#if !CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
// Without it, EXT_RAM_BSS_ATTR silently falls back to internal RAM.
#error "logbuf needs CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y"
#endif

#define LOGBUF_MAGIC 0x4c4f4721 // "LOG!"
#define LOGBUF_MASK (LOGBUF_SIZE - 1)
#define LOGBUF_LINE_MAX 256

static_assert((LOGBUF_SIZE & LOGBUF_MASK) == 0, "LOGBUF_SIZE must be a power of two");

typedef struct {
  uint32_t magic;
  uint32_t size;
  // Position (in the log stream) of the next byte to write.
  uint32_t head;
  // Number of valid bytes before head (<= LOGBUF_SIZE).
  uint32_t used;
  uint32_t boot_count;
  // Detects a header that is garbage after power-on.
  uint32_t check;
  char data[LOGBUF_SIZE];
} logbuf_t;

static EXT_RAM_NOINIT_ATTR logbuf_t s_log;
static EXT_RAM_BSS_ATTR char s_line[LOGBUF_LINE_MAX];
static EXT_RAM_BSS_ATTR char s_mg_line[LOGBUF_LINE_MAX];
static size_t s_mg_len;

static SemaphoreHandle_t s_mutex;
static vprintf_like_t s_orig_vprintf;
static volatile bool s_in_panic;
static const char* volatile s_abort_details;

static uint32_t header_check(void)
{
  return 0xa5a5a5a5 ^ s_log.magic ^ s_log.size ^ (s_log.head * 2654435761u)
    ^ (s_log.used << 7) ^ (s_log.boot_count << 13);
}

// Caller must hold s_mutex, or be the panic handler (other CPU stalled).
static void append(const char* p, size_t n)
{
  while (n > 0) {
    size_t off = s_log.head & LOGBUF_MASK;
    size_t chunk = std::min(n, (size_t) LOGBUF_SIZE - off);
    memcpy(&s_log.data[off], p, chunk);
    s_log.head += chunk;
    s_log.used = std::min<uint32_t>(s_log.used + chunk, LOGBUF_SIZE);
    p += chunk;
    n -= chunk;
  }
  s_log.check = header_check();
}

static bool lock(TickType_t timeout)
{
  if (s_mutex == NULL || xPortInIsrContext() ||
      xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
    return false;
  }
  return xSemaphoreTake(s_mutex, timeout) == pdTRUE;
}

static void unlock(void)
{
  xSemaphoreGive(s_mutex);
}

void logbuf_write(const char* data, size_t len)
{
  // Short timeout: never stall a task for long just because of logging.
  if (lock(pdMS_TO_TICKS(20))) {
    append(data, len);
    unlock();
  }
}

void logbuf_flush(void)
{
  if (spi_flash_cache_enabled()) {
    esp_psram_extram_writeback_cache();
  }
}

static int log_vprintf(const char* fmt, va_list ap)
{
  va_list copy;
  va_copy(copy, ap);
  int ret = s_orig_vprintf(fmt, ap);

  if (lock(pdMS_TO_TICKS(20))) {
    int n = vsnprintf(s_line, sizeof(s_line), fmt, copy);
    if (n > 0) {
      if ((size_t) n >= sizeof(s_line)) {
        // Truncated; keep the line terminated.
        n = sizeof(s_line) - 1;
        s_line[n - 1] = '\n';
      }
      append(s_line, n);
    }
    unlock();
  }
  va_end(copy);
  return ret;
}

static void on_shutdown(void)
{
  static const char msg[] = "----- esp_restart() -----\n";
  logbuf_write(msg, sizeof(msg) - 1);
}

const char* logbuf_reset_reason(void)
{
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external pin";
    case ESP_RST_SW:        return "software (esp_restart)";
    case ESP_RST_PANIC:     return "panic/exception";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "unknown";
  }
}

void init_logbuf(void)
{
  bool valid = s_log.magic == LOGBUF_MAGIC &&
    s_log.size == LOGBUF_SIZE &&
    s_log.used <= LOGBUF_SIZE &&
    s_log.check == header_check();
  if (!valid) {
    s_log.magic = LOGBUF_MAGIC;
    s_log.size = LOGBUF_SIZE;
    s_log.head = 0;
    s_log.used = 0;
    s_log.boot_count = 0;
  }
  s_log.boot_count++;

  char banner[128];
  int n = snprintf(banner, sizeof(banner),
                   "\n===== Boot #%u, reset reason: %s%s =====\n",
                   (unsigned) s_log.boot_count, logbuf_reset_reason(),
                   valid ? "" : " (log buffer was empty)");
  // Still single-threaded as far as this buffer is concerned.
  append(banner, std::min((size_t) n, sizeof(banner) - 1));

  s_mutex = xSemaphoreCreateMutex();
  s_orig_vprintf = esp_log_set_vprintf(log_vprintf);
  esp_register_shutdown_handler(on_shutdown);
}

size_t logbuf_available(uint32_t since, bool has_since)
{
  size_t n = 0;
  if (lock(pdMS_TO_TICKS(1000))) {
    if (!has_since || (uint32_t) (s_log.head - since) > s_log.used) {
      n = s_log.used;
    } else {
      n = s_log.head - since;
    }
    unlock();
  }
  return n;
}

size_t logbuf_read(uint32_t since, bool has_since, char* out, size_t out_size,
                   uint32_t* start, uint32_t* next, uint32_t* boot)
{
  if (!lock(pdMS_TO_TICKS(1000))) {
    *start = *next = since;
    *boot = 0;
    return 0;
  }

  // Unsigned arithmetic also handles "since" being ahead of head (a stale
  // position from before a power cycle): the difference is then huge.
  if (!has_since || (uint32_t) (s_log.head - since) > s_log.used) {
    since = s_log.head - s_log.used;
  }
  size_t n = std::min((size_t) (s_log.head - since), out_size);

  size_t off = since & LOGBUF_MASK;
  size_t first = std::min(n, (size_t) LOGBUF_SIZE - off);
  memcpy(out, &s_log.data[off], first);
  memcpy(out + first, &s_log.data[0], n - first);

  *start = since;
  *next = since + n;
  *boot = s_log.boot_count;
  unlock();
  return n;
}

void logbuf_mg_putchar(char c, void* param)
{
  putchar(c);
  if (c == '\r') {
    return;
  }
  s_mg_line[s_mg_len++] = c;
  if (c == '\n' || s_mg_len == sizeof(s_mg_line)) {
    logbuf_write(s_mg_line, s_mg_len);
    s_mg_len = 0;
  }
}

//--------------------------------------------------------------------------
// Link-time wrappers (-Wl,--wrap=...). See CMakeLists.txt.

extern "C" {

void __real_esp_restart_noos(void) __attribute__((noreturn));
void __real_panic_abort(const char* details) __attribute__((noreturn));
void __real_esp_panic_handler(panic_info_t* info);
#if !CONFIG_ESP_SYSTEM_PANIC_SILENT_REBOOT
void __real_panic_print_char(const char c);
void __real_panic_print_str(const char* str);
void __real_panic_print_hex(int h);
void __real_panic_print_dec(int d);
#endif

// May run with the flash cache disabled (e.g. brownout ISR), hence IRAM.
void IRAM_ATTR __attribute__((noreturn)) __wrap_esp_restart_noos(void)
{
  // If the cache is off, PSRAM can't be written back (and a flash operation
  // may be in progress); skip rather than risk anything.
  if (spi_flash_cache_enabled()) {
    esp_psram_extram_writeback_cache();
  }
  __real_esp_restart_noos();
}

void IRAM_ATTR __attribute__((noreturn)) __wrap_panic_abort(const char* details)
{
  s_abort_details = details;
  __real_panic_abort(details);
}

// In the panic handler, the other CPU is stalled and the mutex can't be
// used, so append without locking.
static void panic_capture(const char* p, size_t n)
{
  if (s_in_panic && spi_flash_cache_enabled()) {
    append(p, n);
  }
}

static void panic_capture_str(const char* s)
{
  if (s != NULL) {
    panic_capture(s, strlen(s));
  }
}

void __wrap_esp_panic_handler(panic_info_t* info)
{
  s_in_panic = true;

  // The handler prints this header through functions in its own file, which
  // the linker can't wrap, so write an equivalent one here.
  char core[] = "0";
  core[0] += info->core;
  panic_capture_str("\n***** Guru Meditation Error: Core ");
  panic_capture_str(core);
  if (g_panic_abort) {
    panic_capture_str(": ");
    panic_capture_str(s_abort_details != NULL ? s_abort_details : "abort()");
  } else {
    if (info->reason != NULL) {
      panic_capture_str(" panic'ed (");
      panic_capture_str(info->reason);
      panic_capture_str(")");
    }
    if (info->description != NULL) {
      panic_capture_str(". ");
      panic_capture_str(info->description);
    }
  }
  panic_capture_str(" *****\n");

  __real_esp_panic_handler(info);
}

#if !CONFIG_ESP_SYSTEM_PANIC_SILENT_REBOOT
void __wrap_panic_print_char(const char c)
{
  __real_panic_print_char(c);
  panic_capture(&c, 1);
}

void __wrap_panic_print_str(const char* str)
{
  __real_panic_print_str(str);
  panic_capture_str(str);
}

void __wrap_panic_print_hex(int h)
{
  __real_panic_print_hex(h);
  // Same format as panic_print_hex(): 8 digits, no "0x".
  static const char digits[] = "0123456789abcdef";
  char buf[8];
  for (int i = 0; i < 8; i++) {
    buf[i] = digits[((uint32_t) h >> (28 - 4 * i)) & 0xf];
  }
  panic_capture(buf, sizeof(buf));
}

void __wrap_panic_print_dec(int d)
{
  __real_panic_print_dec(d);
  // Same format as panic_print_dec(): two characters, space-padded.
  char buf[2] = { d / 10 == 0 ? ' ' : (char) ('0' + d / 10), (char) ('0' + d % 10) };
  panic_capture(buf, sizeof(buf));
}
#endif

} // extern "C"
