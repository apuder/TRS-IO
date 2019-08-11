
#include <time.h>
#include <sys/time.h>
#include "storage.h"
#include "lwip/apps/sntp.h"

#define KEY_TZ "tz"

void set_timezone(const char* tz) {
  storage_set_str(KEY_TZ, tz);
  setenv("TZ", tz, 1);
  tzset();
}

void init_time() {
  if (storage_has_key(KEY_TZ)) {
    char tz[33];
    size_t len = sizeof(tz);
    storage_get_str(KEY_TZ, tz, &len);
    setenv("TZ", tz, 1);
    tzset();
  }
  
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();
}
