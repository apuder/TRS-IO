
#include <time.h>
#include <sys/time.h>
#include "settings.h"
#include "wifi.h"
#include "ntp_sync.h"
#include "lwip/apps/sntp.h"
#include "timezones.h"


void set_timezone() {
  string& tz = settings_get_tz();
  if (tz.empty()) {
    return;
  }

  // Convert from a friendly timezone (like "America/Los_Angeles").
  string posix = friendlyTimezoneToPosix(tz);
  if (posix.empty()) {
      posix = tz;
  }

  setenv("TZ", posix.c_str(), 1);
  tzset();
}

void init_time() {
  set_timezone();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_init();
}
