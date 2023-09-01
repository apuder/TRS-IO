
#include <time.h>
#include <sys/time.h>
#include "settings.h"
#include "wifi.h"
#include "ntp_sync.h"
#include "lwip/apps/sntp.h"


void set_timezone() {
  string& tz = settings_get_tz();
  if (tz.empty()) {
    return;
  }
  string patched = tz;
  for (int i = 0; i < patched.size(); i++) {
    if (patched[i] == '-') {
      patched[i] = '+';
    } else if (patched[i] == '+') {
      patched[i] = '-';
    }
  }
  setenv("TZ", patched.c_str(), 1);
  tzset();
}

void init_time() {
  set_timezone();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();
}
