
#include "retrostore.h"
#include "wifi.h"
#include "spi.h"
#include "printer.h"
#include "ntp_sync.h"
#include "trs-fs.h"
#ifdef CONFIG_TRS_IO_PP
#include "fs-roms.h"
#include "fs-spiffs.h"
#endif
#include "smb.h"
#include "ota.h"
#include "led.h"
#include "io.h"
#include "settings.h"
#include "event.h"
#include "ntp_sync.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "esp_mock.h"
#include "version.h"
#include "mdns.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mongoose.h"
#include "web_debugger.h"
#include <string.h>
#include "cJSON.h"


#define SSID "TRS-IO"
#define TAG "WIFI"


static uint8_t status = RS_STATUS_WIFI_CONNECTING;

uint8_t* get_wifi_status()
{
  return &status;
}


static char ip[16] = {'-', '\0'};

const char* get_wifi_ip()
{
  return ip;
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG,"connect to the AP fail");
    status = RS_STATUS_WIFI_NOT_CONNECTED;
    evt_signal(EVT_WIFI_DOWN);
    esp_wifi_connect();
    set_led(true, false, false, false, false);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Got IP: %s", ip);
    status = RS_STATUS_WIFI_CONNECTED;
    set_led(false, true, false, false, true);
    evt_signal(EVT_WIFI_UP);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    evt_signal(EVT_START_MG);
    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    ESP_LOGI(TAG, "Station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    ESP_LOGI(TAG, "Station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    evt_signal(EVT_WIFI_DOWN);
    status = RS_STATUS_WIFI_NOT_CONNECTED;
    ip[0] = '-';
    ip[1] = '\0';
    set_led(true, false, false, false, false);
    esp_wifi_connect();
  }
}



void wifi_init_ap()
{
  wifi_config_t wifi_config = {
    .ap = {0}
  };

  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  strcpy((char*) wifi_config.ap.ssid, SSID);
  wifi_config.ap.ssid_len = strlen(SSID);
  strcpy((char*) wifi_config.ap.password, "");
  wifi_config.ap.max_connection = 1;
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
}

static void wifi_init_sta()
{
  wifi_config_t wifi_config = {
    .sta = {0},
  };

  status = RS_STATUS_WIFI_CONNECTING;

  string& ssid = settings_get_wifi_ssid();
  string& passwd = settings_get_wifi_passwd();
  strcpy((char*) wifi_config.sta.ssid, ssid.c_str());
  strcpy((char*) wifi_config.sta.password, passwd.c_str());

  ESP_LOGI(TAG, "wifi_init_sta: SSID=%s", (char*) wifi_config.sta.ssid);
  ESP_LOGI(TAG, "wifi_init_sta: Passwd=%s", (char*) wifi_config.sta.password);

  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}


void init_wifi()
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                ESP_EVENT_ANY_ID,
                                                event_handler,
                                                NULL,
                                                NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                IP_EVENT_STA_GOT_IP,
                                                event_handler,
                                                NULL,
                                                NULL));

#ifdef CONFIG_TRS_IO_USE_COMPILE_TIME_WIFI_CREDS
  storage_set_str(WIFI_KEY_SSID, CONFIG_TRS_IO_SSID);
  storage_set_str(WIFI_KEY_PASSWD, CONFIG_TRS_IO_PASSWD);
#endif
  if (settings_has_wifi_credentials()) {
    wifi_init_sta();
  } else {
    status = RS_STATUS_WIFI_NOT_CONFIGURED;
    set_led(false, true, true, true, false);
    wifi_init_ap();
  }
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}
