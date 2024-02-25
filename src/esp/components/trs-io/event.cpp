

#include "event.h"
#include "trs-fs.h"
#include "spi.h"

#define ESP_STATUS_ESP_READY   (1 << 0)
#define ESP_STATUS_WIFI_UP     (1 << 1)
#define ESP_STATUS_SMB_MOUNTED (1 << 2)
#define ESP_STATUS_SD_MOUNTED  (1 << 3)

static uint8_t esp_status = 0;


static EventGroupHandle_t eg;

void evt_send_esp_status()
{
  spi_set_esp_status(esp_status);
}

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
  uint8_t new_esp_status = esp_status;

  if (evt_check(EVT_ESP_READY) != 0) {
    new_esp_status |= ESP_STATUS_ESP_READY;
  }
  if (evt_check(EVT_WIFI_UP) != 0) {
    init_trs_fs_smb();
    if (get_smb_err_msg() == NULL) {
      new_esp_status |= ESP_STATUS_SMB_MOUNTED;
    }
    new_esp_status |= ESP_STATUS_WIFI_UP;
    evt_signal(EVT_START_MG | EVT_CHECK_OTA);
  }
  if (evt_check(EVT_WIFI_DOWN) != 0) {
    new_esp_status &= ~(ESP_STATUS_WIFI_UP | ESP_STATUS_SMB_MOUNTED);
  }
  if (evt_check(EVT_SD_MOUNTED) != 0) {
    new_esp_status |= ESP_STATUS_SD_MOUNTED;
  }
  if (evt_check(EVT_SD_UNMOUNTED) != 0) {
    new_esp_status &= ~ESP_STATUS_SD_MOUNTED;
  }
  if (esp_status != new_esp_status) {
    esp_status = new_esp_status;
    spi_set_esp_status(esp_status);
  }
}

void init_events()
{
  eg = xEventGroupCreate();
}
