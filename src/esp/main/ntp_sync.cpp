
#include <time.h>
#include <sys/time.h>
#include "storage.h"
#include "esp_sntp.h"
#include "ntp_sync.h"

#define KEY_TZ "tz"

void set_timezone(const char* tz) {
  storage_set_str(KEY_TZ, tz);
  setenv("TZ", tz, 1);
  tzset();
}

void init_time() {
  if (storage_has_key(KEY_TZ)) {
    char tz[33];
    storage_get_str(KEY_TZ, tz, sizeof(tz));
    setenv("TZ", tz, 1);
    tzset();
  }
  
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();
}
