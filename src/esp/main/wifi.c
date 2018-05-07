
#include "led.h"
#include "storage.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#define WIFI_KEY_SSID "ssid"
#define WIFI_KEY_PASSWD "passwd"

static const char *TAG = "RetroStore";

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  switch(event->event_id) {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGI(TAG, "got ip:%s",
             ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
    set_led(false, true, false, false, true);
    break;
  case SYSTEM_EVENT_AP_STACONNECTED:
    ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
             MAC2STR(event->event_info.sta_connected.mac),
             event->event_info.sta_connected.aid);
    break;
  case SYSTEM_EVENT_AP_STADISCONNECTED:
    ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
             MAC2STR(event->event_info.sta_disconnected.mac),
             event->event_info.sta_disconnected.aid);
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    esp_wifi_connect();
    set_led(true, false, false, false, false);
    break;
  default:
    break;
  }
  return ESP_OK;
}

void wifi_init_sta()
{
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = "PEETS",
      .password = ""
    },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void init_wifi()
{
  if (storage_has_key(WIFI_KEY_SSID) && storage_has_key(WIFI_KEY_PASSWD)) {
  }
  wifi_init_sta();
}
