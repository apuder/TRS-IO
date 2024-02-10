

#include "event.h"
#include "trs-fs.h"


static EventGroupHandle_t eg;

void evt_signal(EventBits_t bits)
{
  xEventGroupSetBits(eg, bits);
}

EventBits_t evt_wait(EventBits_t bits)
{
  return xEventGroupWaitBits(eg, bits, pdTRUE, pdFALSE, portMAX_DELAY) & bits;
}

EventBits_t evt_check(EventBits_t bits)
{
  return xEventGroupWaitBits(eg, bits, pdTRUE, pdFALSE, 0) & bits;
}

void check_events()
{
  if (evt_check(EVT_WIFI_UP) != 0) {
    init_trs_fs_smb();
    evt_signal(EVT_START_MG | EVT_CHECK_OTA);
  }
}

void init_events()
{
  eg = xEventGroupCreate();
}
