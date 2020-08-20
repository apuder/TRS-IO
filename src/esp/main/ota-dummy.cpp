
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi.h"
#include "retrostore.h"


void switch_to_factory()
{
}

static void ota_task(void* p)
{
  start_mg(true);
  vTaskDelete(NULL);
}

void init_ota()
{
  xTaskCreatePinnedToCore(ota_task, "ota", 4096, NULL, 1, NULL, 0);
}
