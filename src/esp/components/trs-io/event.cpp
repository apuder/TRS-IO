

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "event.h"

static const int WIFI_UP_BIT = BIT0;

static EventGroupHandle_t eg;

void evt_signal_wifi_up() {
  xEventGroupSetBits(eg, WIFI_UP_BIT);
}

void evt_wait_wifi_up() {
  xEventGroupWaitBits(eg, WIFI_UP_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
}

void init_events() {
  eg = xEventGroupCreate();
}
