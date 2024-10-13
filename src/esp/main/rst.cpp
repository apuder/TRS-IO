
#include "rst.h"
#include "keyb.h"
#include "esp_system.h"

void reboot_trs_io()
{
#ifdef CONFIG_TRS_IO_PP
  reset_keyb();
#endif
  esp_restart();
}