
#include "retrostore.h"
#include "wifi.h"
#include "ota.h"
#include "led.h"
#include "storage.h"
#include "ntp_sync.h"
#include "esp_wifi.h"
#include "esp_mock.h"
#include "mdns.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "mongoose.h"
#include <string.h>

#define WIFI_KEY_SSID "ssid"
#define WIFI_KEY_PASSWD "passwd"

#define SSID "TRS-IO"
#define MDNS_NAME "trs-io"

static const char* TAG = "TRS-IO";

extern unsigned char index_html[];
extern unsigned int index_html_len;
extern unsigned char status_html[];
extern unsigned int status_html_len;

static uint8_t status = RS_STATUS_WIFI_CONNECTING;

static char ssid[33];
static char passwd[33];

static bool reboot;

uint8_t* get_wifi_status()
{
  return &status;
}

const char* get_wifi_ssid()
{
  static char ssid[33];
  
  storage_get_str(WIFI_KEY_SSID, ssid, sizeof(ssid));
  return ssid;
}

static char* ip = "-";

const char* get_wifi_ip()
{
  return ip;
}

static esp_err_t event_handler(void* ctx, system_event_t* event)
{
  switch(event->event_id) {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    ip = ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip);
    ESP_LOGI(TAG, "got ip:%s", ip);
    status = RS_STATUS_WIFI_CONNECTED;
    trigger_ota_check();
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
    status = RS_STATUS_WIFI_NOT_CONNECTED;
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    status = RS_STATUS_WIFI_NOT_CONNECTED;
    esp_wifi_connect();
    set_led(true, false, false, false, false);
    break;
  default:
    break;
  }
  return ESP_OK;
}

void set_wifi_credentials(const char* ssid, const char* passwd)
{
  // Store credentials and reboot
  storage_set_str(WIFI_KEY_SSID, ssid);
  storage_set_str(WIFI_KEY_PASSWD, passwd);
  esp_restart();
}

void mongoose_event_handler(struct mg_connection* nc,
                            int event,
                            void* eventData)
{
  switch (event) {
  case MG_EV_HTTP_REQUEST:
    {
      struct http_message* message = (struct http_message*) eventData;
      const char* uri = message->uri.p;
      unsigned char* response = index_html;
      unsigned int response_len = index_html_len;
      if (strncmp(uri, "/config", 7) == 0) {
        int l1 = mg_get_http_var(&message->body, "ssid", ssid, sizeof(ssid));
        int l2 = mg_get_http_var(&message->body, "passwd", passwd, sizeof(passwd));
        if ((l1 >= 0) && (l2 >= 0)) {
          storage_set_str(WIFI_KEY_SSID, ssid);
          storage_set_str(WIFI_KEY_PASSWD, passwd);
          response = status_html;
          response_len = status_html_len;
          reboot = true;
        }

        char tz[33];
        int l3 = mg_get_http_var(&message->body, "tz", tz, sizeof(tz));
        if (l3 > 0) {
          set_timezone(tz);
        }
      }
      mg_send_head(nc, 200, response_len, "Content-Type: text/html");
      mg_send(nc, response, response_len);
      nc->flags |= MG_F_SEND_AND_CLOSE;
    }
    break;
  case MG_EV_CLOSE:
    {
      if (reboot) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart();
      }
    }
    break;
  }
}

void mg_task(void* p)
{
  struct mg_mgr mgr;
  
  // Start mDNS service
  ESP_ERROR_CHECK(mdns_init());
  ESP_ERROR_CHECK(mdns_hostname_set(MDNS_NAME));
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

  // Start Mongoose
  mg_mgr_init(&mgr, NULL);
  reboot = false;
  struct mg_connection *c = mg_bind(&mgr, ":80", mongoose_event_handler);
  mg_set_protocol_http_websocket(c);

  while(true) {
    mg_mgr_poll(&mgr, 1000);
  }
}

void wifi_init_ap()
{
  wifi_config_t wifi_config;
  
  strcpy((char*) wifi_config.ap.ssid, SSID);
  wifi_config.ap.ssid_len = strlen(SSID);
  strcpy((char*) wifi_config.ap.password, "");
  wifi_config.ap.max_connection = 1;
  wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  
  ESP_LOGI(TAG, "wifi_init_ap finished.SSID:%s", SSID);
}

static void wifi_init_sta()
{
  wifi_config_t wifi_config = {
    .sta = {},
  };

  storage_get_str(WIFI_KEY_SSID, ssid, sizeof(ssid));
  storage_get_str(WIFI_KEY_PASSWD, passwd, sizeof(passwd));
  
  ESP_LOGI(TAG, "wifi_init_sta: SSID=%s", ssid);
  ESP_LOGI(TAG, "wifi_init_sta: Passwd=%s", passwd);

  strcpy((char*) wifi_config.sta.ssid, ssid);
  strcpy((char*) wifi_config.sta.password, passwd);
  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void init_wifi()
{
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  if (storage_has_key(WIFI_KEY_SSID) && storage_has_key(WIFI_KEY_PASSWD)) {
    status = RS_STATUS_WIFI_CONNECTING;
    wifi_init_sta();
  } else {
    status = RS_STATUS_WIFI_NOT_CONFIGURED;
    set_led(false, true, true, true, false);
    wifi_init_ap();
    xTaskCreatePinnedToCore(mg_task, "mg", 3000, NULL, 1, NULL, 0);
  }
}
