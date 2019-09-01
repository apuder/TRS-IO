
#include <time.h>
#include <sys/time.h>
#include "storage.h"
#include "ntp_sync.h"
#include "lwip/apps/sntp.h"


void set_timezone() {
  if (!storage_has_key(NTP_KEY_TZ)) {
    return;
  }
  static char tz[33];
  size_t len = sizeof(tz);
  storage_get_str(NTP_KEY_TZ, tz, &len);
  char* p = tz;
  while(*p != '\0') {
    if (*p == '-') {
      *p = '+';
    } else if (*p == '+') {
      *p = '-';
    }
    p++;
  }
  setenv("TZ", tz, 1);
  tzset();
}

void init_time() {
  set_timezone();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();
}
