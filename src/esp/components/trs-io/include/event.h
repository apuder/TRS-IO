
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define EVT_ESP_READY            (1 << 0)
#define EVT_WIFI_UP              (1 << 1)
#define EVT_WIFI_DOWN            (1 << 2)
#define EVT_CHECK_OTA            (1 << 3)
#define EVT_START_MG             (1 << 4)
#define EVT_SD_MOUNTED           (1 << 5)
#define EVT_SD_UNMOUNTED         (1 << 6)
#define EVT_CASS_MOTOR_ON        (1 << 7)
#define EVT_CASS_MOTOR_OFF       (1 << 8)
#define EVT_I2S_START            (1 << 9)
#define EVT_I2S_STOP             (1 << 10)
#define EVT_FS_MOUNTED           (1 << 11)
#define EVT_XFER_RESULT_READY    (1 << 12)
#define EVT_XFER_RESULT_CONSUMED (1 << 13)
#define EVT_XFER_Z80_WAITING     (1 << 14)

void evt_signal(EventBits_t bits);
EventBits_t evt_wait(EventBits_t bits);
EventBits_t evt_check(EventBits_t bits);
void check_events();
void evt_send_esp_status();
void init_events();
