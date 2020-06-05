
#include "retrostore.h"
#include "wifi.h"
#include "ntp_sync.h"
#include "trs-fs.h"
#include "smb.h"
#include "ota.h"
#include "led.h"
#include "io.h"
#include "storage.h"
#include "event.h"
#include "ntp_sync.h"
#include "esp_wifi.h"
#include "esp_mock.h"
#include "version.h"
#include "mdns.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mongoose.h"
#include <string.h>
#include "cJSON.h"

#define WIFI_KEY_SSID "ssid"
#define WIFI_KEY_PASSWD "passwd"

#define SSID "TRS-IO"
#define MDNS_NAME "trs-io"

static const char* TAG = "TRS-IO";

extern unsigned char index_html[];
extern unsigned int index_html_len;

static uint8_t status = RS_STATUS_WIFI_CONNECTING;


uint8_t* get_wifi_status()
{
  return &status;
}

const char* get_wifi_ssid()
{
  static char ssid[33];

  size_t len = sizeof(ssid);
  storage_get_str(WIFI_KEY_SSID, ssid, &len);
  return ssid;
}

static char* ip = (char*) "-";

const char* get_wifi_ip()
{
  return ip;
}

static char buf[64];


static esp_err_t event_handler(void* ctx, system_event_t* event)
{
  switch(event->event_id) {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
    status = RS_STATUS_WIFI_CONNECTED;
    evt_signal_wifi_up();
    set_led(false, true, false, false, true);
    init_trs_fs();
    break;
  case SYSTEM_EVENT_AP_STACONNECTED:
    ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d",
             MAC2STR(event->event_info.sta_connected.mac),
             event->event_info.sta_connected.aid);
    evt_signal_wifi_up();
    break;
  case SYSTEM_EVENT_AP_STADISCONNECTED:
    ESP_LOGI(TAG, "station:" MACSTR "leave, AID=%d",
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

static bool extract_post_param(struct http_message* message,
                               const char* param,
                               const char* nvs_key)
{
  static char buf2[sizeof(buf)];
  
  int len = mg_get_http_var(&message->body, param, buf, sizeof(buf));
  if (!storage_has_key(nvs_key)) {
    storage_set_str(nvs_key, buf);
    return true;
  }
  size_t len2 = sizeof(buf2);
  storage_get_str(nvs_key, buf2, &len2);
  if (strcmp(buf, buf2) == 0) {
    return false;
  }
  storage_set_str(nvs_key, buf);
  return true;
}


//-----------------------------------------------------------------
// Web server

static struct {
  char* ssid;
  char* passwd;
  char* tz;
  char* smb_url;
  char* smb_user;
  char* smb_passwd;
} config;

static void copy_config_from_nvs(const char* key, char** value)
{
  size_t len;
  
  if (*value != NULL) {
    free(*value);
  }
  *value = NULL;
  if (storage_has_key(key, &len)) {
    *value = (char*) malloc(len);
    storage_get_str(key, *value, &len);
  }
}

static void copy_config_from_nvs()
{
  copy_config_from_nvs(WIFI_KEY_SSID, &config.ssid);
  copy_config_from_nvs(WIFI_KEY_PASSWD, &config.passwd);
  copy_config_from_nvs(NTP_KEY_TZ, &config.tz);
  copy_config_from_nvs(SMB_KEY_URL, &config.smb_url);
  copy_config_from_nvs(SMB_KEY_USER, &config.smb_user);
  copy_config_from_nvs(SMB_KEY_PASSWD, &config.smb_passwd);
}

static bool mongoose_handle_config(struct http_message* message,
                                   char** response,
                                   unsigned int* response_len,
                                   const char** content_type)
{
  bool reboot = false;
  bool smb_connect = false;

  *response = "";
  *response_len = 0;
  *content_type = "Content-Type: text/plain";

  reboot |= extract_post_param(message, "ssid", WIFI_KEY_SSID);
  reboot |= extract_post_param(message, "passwd", WIFI_KEY_PASSWD);
  extract_post_param(message, "tz", NTP_KEY_TZ);
  set_timezone();

  smb_connect |= extract_post_param(message, "smb_url", SMB_KEY_URL);
  smb_connect |= extract_post_param(message, "smb_user", SMB_KEY_USER);
  smb_connect |= extract_post_param(message, "smb_passwd", SMB_KEY_PASSWD);

  if (smb_connect) {
    init_trs_fs();
  }

  copy_config_from_nvs();

  return reboot;
}

static void mongoose_handle_status(struct http_message* message,
                                   char** response,
                                   unsigned int* response_len,
                                   const char** content_type)
{
  static char* resp = NULL;

  if (resp != NULL) {
    free(resp);
    resp = NULL;
  }
  
  cJSON* s = cJSON_CreateObject();
  cJSON_AddNumberToObject(s, "hardware_rev", TRS_IO_HARDWARE_REVISION);
  cJSON_AddNumberToObject(s, "vers_major", TRS_IO_VERSION_MAJOR);
  cJSON_AddNumberToObject(s, "vers_minor", TRS_IO_VERSION_MINOR);
  cJSON_AddNumberToObject(s, "wifi_status", status);
  if (config.ssid != NULL) {
    cJSON_AddStringToObject(s, "ssid", config.ssid);
  }
  if (config.passwd != NULL) {
    cJSON_AddStringToObject(s, "passwd", config.passwd);
  }
  if (config.tz != NULL) {
    cJSON_AddStringToObject(s, "tz", config.tz);
  }
  if (config.smb_url != NULL) {
    cJSON_AddStringToObject(s, "smb_url", config.smb_url);
  }
  if (config.smb_user != NULL) {
    cJSON_AddStringToObject(s, "smb_user", config.smb_user);
  }
  if (config.smb_passwd != NULL) {
    cJSON_AddStringToObject(s, "smb_passwd", config.smb_passwd);
  }

  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  char* time;
  asprintf(&time, "%d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  cJSON_AddStringToObject(s, "time", time);
  free(time);

  const char* smb_err = get_smb_err_msg();
  if (smb_err != NULL) {
    cJSON_AddStringToObject(s, "smb_err", smb_err);
  }

  resp = cJSON_PrintUnformatted(s);
  *response = resp;
  *response_len = strlen(resp);
  cJSON_Delete(s);

  *content_type = "Content-Type: application/json";
}

static void mongoose_event_handler(struct mg_connection* nc,
                                   int event,
                                   void* eventData)
{
  static bool reboot = false;
  
  switch (event) {
  case MG_EV_HTTP_REQUEST:
    {
      struct http_message* message = (struct http_message*) eventData;
      const char* uri = message->uri.p;
      char* response = (char*) index_html;
      unsigned int response_len = index_html_len;
      const char* content_type = "Content-Type: text/html";

      if (strncmp(uri, "/config", 7) == 0) {
        reboot = mongoose_handle_config(message, &response,
                                        &response_len, &content_type);
      }

      if (strncmp(uri, "/status", 7) == 0) {
        mongoose_handle_status(message, &response,
                               &response_len, &content_type);
      }
      
      mg_send_head(nc, 200, response_len, content_type);
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

static void mg_task(void* p)
{
  struct mg_mgr mgr;

  config.ssid = NULL;
  config.passwd = NULL;
  config.tz = NULL;
  config.smb_url = NULL;
  config.smb_user = NULL;
  config.smb_passwd = NULL;
  copy_config_from_nvs();

  // Start mDNS service
  ESP_ERROR_CHECK(mdns_init());
  ESP_ERROR_CHECK(mdns_hostname_set(MDNS_NAME));
  mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

  // Start Mongoose
  mg_mgr_init(&mgr, NULL);
  struct mg_connection *c = mg_bind(&mgr, ":80", mongoose_event_handler);
  mg_set_protocol_http_websocket(c);

  while(true) {
    mg_mgr_poll(&mgr, 1000);
  }
}

void start_mg()
{
  xTaskCreatePinnedToCore(mg_task, "mg", 5000, NULL, 1, NULL, 0);
}

//--------------------------------------------------------------------------

void wifi_init_ap()
{
  wifi_config_t wifi_config = {0};
  
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

  status = RS_STATUS_WIFI_CONNECTING;

  size_t len = sizeof(wifi_config.sta.ssid);
  storage_get_str(WIFI_KEY_SSID, (char*) wifi_config.sta.ssid, &len);
  len = sizeof(wifi_config.sta.password);
  storage_get_str(WIFI_KEY_PASSWD, (char*) wifi_config.sta.password, &len);

  ESP_LOGI(TAG, "wifi_init_sta: SSID=%s", (char*) wifi_config.sta.ssid);
  ESP_LOGI(TAG, "wifi_init_sta: Passwd=%s", (char*) wifi_config.sta.password);

  esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

void init_wifi()
{
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
  ESP_ERROR_CHECK(esp_netif_init());

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

#ifdef CONFIG_TRS_IO_USE_COMPILE_TIME_WIFI_CREDS
  storage_set_str(WIFI_KEY_SSID, CONFIG_TRS_IO_SSID);
  storage_set_str(WIFI_KEY_PASSWD, CONFIG_TRS_IO_PASSWD);
#endif
  if (storage_has_key(WIFI_KEY_SSID) && storage_has_key(WIFI_KEY_PASSWD)) {
    wifi_init_sta();
  } else {
    status = RS_STATUS_WIFI_NOT_CONFIGURED;
    set_led(false, true, true, true, false);
    wifi_init_ap();
  }
}
