#pragma once

// Ring buffer of recent log output, kept in PSRAM, so that logs can be read
// over HTTP (GET /log) when there is no serial console.
//
// Captured:
//   - everything printed through ESP_LOGx (after init_logbuf() is called),
//   - Mongoose's MG_xxx log lines (when logbuf_mg_putchar is installed),
//   - the panic handler's register dump and backtrace on a crash.
//
// The buffer lives in a PSRAM section that is not cleared at startup, so the
// log of the previous run survives a software reset, a panic, or a watchdog
// reset (but not a power cycle). Every boot appends a "===== Boot" banner
// with the reset reason.

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Size of the ring buffer in bytes. Must be a power of two.
#define LOGBUF_SIZE (128 * 1024)

// Call once, as early as possible in app_main().
void init_logbuf(void);

// Append raw text. Safe to call from any task (not from an ISR).
void logbuf_write(const char* data, size_t len);

// Returns how many bytes logbuf_read() would return right now for "since".
size_t logbuf_available(uint32_t since, bool has_since);

// Copy log text into "out" (at most out_size bytes) and return the number of
// bytes copied.
//
// Positions are byte offsets into the log stream since the buffer was
// initialized. If has_since is false, or "since" has already been overwritten
// (or is in the future, e.g. after a power cycle), copying starts at the
// oldest byte still in the buffer. *start is set to the position of the first
// returned byte, *next to the position to pass as "since" next time, and
// *boot to the boot counter.
size_t logbuf_read(uint32_t since, bool has_since, char* out, size_t out_size,
                   uint32_t* start, uint32_t* next, uint32_t* boot);

// Human-readable reset reason of the current boot.
const char* logbuf_reset_reason(void);

// Mongoose log sink (signature of mg_pfn_t): echoes to stdout like
// Mongoose's default and appends complete lines to the buffer. Install with
// mg_log_set_fn(logbuf_mg_putchar, NULL). Only call from the Mongoose task.
void logbuf_mg_putchar(char c, void* param);

#ifdef __cplusplus
}
#endif
