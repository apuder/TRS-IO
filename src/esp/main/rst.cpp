#include "rst.h"
#include "keyb.h"
#include "logbuf.h"
#include "esp_system.h"

void reboot_trs_io()
{
#ifdef CONFIG_TRS_IO_PP
  // reset_keyb() resets the chip through the RTC watchdog and never returns,
  // so esp_restart() (which writes back the log buffer) is not reached.
  static const char msg[] = "----- reboot_trs_io(): RTC reset -----\n";
  logbuf_write(msg, sizeof(msg) - 1);
  logbuf_flush();
  reset_keyb();
#endif
  esp_restart();
}
