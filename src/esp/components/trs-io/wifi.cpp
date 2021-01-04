
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

static const char* TAG = "TRS-IO wifi";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static uint8_t status = RS_STATUS_WIFI_CONNECTING;


uint8_t* get_wifi_status()
{
  return &status;
}

const char* get_wifi_ssid()
{
  static char ssid[33];

  if (!storage_has_key(WIFI_KEY_SSID)) {
    ssid[0] = '-';
    ssid[1] = '\0';
  } else {
    size_t len = sizeof(ssid);
    storage_get_str(WIFI_KEY_SSID, ssid, &len);
  }
  return ssid;
}

static char* ip = (char*) "-";

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
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
    status = RS_STATUS_WIFI_CONNECTED;
    evt_signal_wifi_up();
    set_led(false, true, false, false, true);
    init_trs_fs();
    break;
  case SYSTEM_EVENT_AP_STACONNECTED:
    ESP_LOGI(TAG, "AP connected. station:" MACSTR " join, AID=%d",
             MAC2STR(event->event_info.sta_connected.mac),
             event->event_info.sta_connected.aid);
    evt_signal_wifi_up();
    break;
  case SYSTEM_EVENT_AP_STADISCONNECTED:
    ESP_LOGI(TAG, "AP disconnected. station:" MACSTR "leave, AID=%d",
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


//-----------------------------------------------------------------
// Web server

static trs_io_wifi_config_t config EXT_RAM_ATTR = {};

static void copy_config_from_nvs(const char* key, char* value, size_t max_len)
{
  size_t len;
  
  *value = '\0';
  if (storage_has_key(key, &len)) {
    assert (len <= max_len + 1);
    storage_get_str(key, value, &len);
  }
}

static void copy_config_from_nvs()
{
  copy_config_from_nvs(WIFI_KEY_SSID, config.ssid, MAX_LEN_SSID);
  copy_config_from_nvs(WIFI_KEY_PASSWD, config.passwd, MAX_LEN_PASSWD);
  copy_config_from_nvs(NTP_KEY_TZ, config.tz, MAX_LEN_TZ);
  copy_config_from_nvs(SMB_KEY_URL, config.smb_url, MAX_LEN_SMB_URL);
  copy_config_from_nvs(SMB_KEY_USER, config.smb_user, MAX_LEN_SMB_USER);
  copy_config_from_nvs(SMB_KEY_PASSWD, config.smb_passwd, MAX_LEN_SMB_PASSWD);
}

trs_io_wifi_config_t* get_wifi_config()
{
  copy_config_from_nvs();
  return &config;
}

static bool extract_post_param(struct http_message* message,
                               const char* param,
                               const char* nvs_key,
                               size_t max_len)
{
  // SMB URL is the longest parameter
  static char buf[MAX_LEN_SMB_URL + 1] EXT_RAM_ATTR;
  static char buf2[sizeof(buf)] EXT_RAM_ATTR;
  
  mg_get_http_var(&message->body, param, buf, sizeof(buf));
  // In case someone tries to force a buffer overflow
  buf[max_len + 1] = '\0';
  if (!storage_has_key(nvs_key)) {
    // Param hasn't been set before
    storage_set_str(nvs_key, buf);
    return true;
  }
  size_t len2;
  storage_get_str(nvs_key, buf2, &len2);
  if (strcmp(buf, buf2) == 0) {
    // Param hasn't changed
    return false;
  }
  storage_set_str(nvs_key, buf);
  return true;
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

  reboot |= extract_post_param(message, "ssid", WIFI_KEY_SSID, MAX_LEN_SSID);
  reboot |= extract_post_param(message, "passwd", WIFI_KEY_PASSWD, MAX_LEN_PASSWD);
  extract_post_param(message, "tz", NTP_KEY_TZ, MAX_LEN_TZ);
  set_timezone();

  smb_connect |= extract_post_param(message, "smb_url", SMB_KEY_URL, MAX_LEN_SMB_URL);
  smb_connect |= extract_post_param(message, "smb_user", SMB_KEY_USER, MAX_LEN_SMB_USER);
  smb_connect |= extract_post_param(message, "smb_passwd", SMB_KEY_PASSWD, MAX_LEN_SMB_PASSWD);

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
  if (config.ssid[0] != '\0') {
    cJSON_AddStringToObject(s, "ssid", config.ssid);
  }
  if (config.passwd[0] != '\0') {
    cJSON_AddStringToObject(s, "passwd", config.passwd);
  }
  if (config.tz[0] != '\0') {
    cJSON_AddStringToObject(s, "tz", config.tz);
  }
  if (config.smb_url[0] != '\0') {
    cJSON_AddStringToObject(s, "smb_url", config.smb_url);
  }
  if (config.smb_user[0] != '\0') {
    cJSON_AddStringToObject(s, "smb_user", config.smb_user);
  }
  if (config.smb_passwd[0] != '\0') {
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
      char* response = (char*) index_html_start;
      unsigned int response_len = index_html_end - index_html_start;
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

static void init_mdns()
{
  // Start mDNS service
  ESP_ERROR_CHECK(mdns_init());
  ESP_ERROR_CHECK(mdns_hostname_set(MDNS_NAME));
  ESP_ERROR_CHECK(mdns_instance_name_set(MDNS_NAME));
  ESP_ERROR_CHECK(mdns_service_add("TRS-IO-WebServer", "_http",
				   "_tcp", 80, NULL, 0));
}

static void mg_task(void* p)
{
  static struct mg_mgr mgr;

  evt_wait_wifi_up();
  ESP_LOGI(TAG, "Starting Mongoose");
  init_time();
  init_mdns();
  copy_config_from_nvs();

  // Start Mongoose
  mg_mgr_init(&mgr, NULL);
  struct mg_connection *c = mg_bind(&mgr, ":80", mongoose_event_handler);
  mg_set_protocol_http_websocket(c);

  while(true) {
    vTaskDelay(1);
    mg_mgr_poll(&mgr, 1000);
  }
}

//--------------------------------------------------------------------------

void wifi_init_ap()
{
  esp_netif_create_default_wifi_ap();
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
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

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
  xTaskCreatePinnedToCore(mg_task, "mg", 6000, NULL, 1, NULL, 0);
}
