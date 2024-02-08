
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define EVT_WIFI_UP   (1 << 0)
#define EVT_WIFI_DOWN (1 << 1)
#define EVT_CHECK_OTA (1 << 2)
#define EVT_START_MG  (1 << 3)

void evt_signal(EventBits_t bits);
EventBits_t evt_wait(EventBits_t bits);
EventBits_t evt_check(EventBits_t bits);
void check_events();
void init_events();
