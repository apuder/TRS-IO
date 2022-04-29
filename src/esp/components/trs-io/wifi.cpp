
#include "retrostore.h"
#include "wifi.h"
#include "printer.h"
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
#include "web_debugger.h"
#include <string.h>
#include "cJSON.h"

#define WIFI_KEY_SSID "ssid"
#define WIFI_KEY_PASSWD "passwd"

#define SSID "TRS-IO"
#define MDNS_NAME "trs-io"

static const char* TAG = "TRS-IO wifi";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t printer_html_start[] asm("_binary_printer_html_start");
extern const uint8_t font_ttf_start[] asm("_binary_AnotherMansTreasureMIII64C_ttf_start");
extern const uint8_t font_ttf_end[] asm("_binary_AnotherMansTreasureMIII64C_ttf_end");

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
    esp_wifi_connect();
    set_led(true, false, false, false, false);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Got IP: %s", ip);
    status = RS_STATUS_WIFI_CONNECTED;
    evt_signal_wifi_up();
    set_led(false, true, false, false, true);
    init_trs_fs_smb();
  } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
    ESP_LOGI(TAG, "Station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    evt_signal_wifi_up();
  } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
    ESP_LOGI(TAG, "Station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
    status = RS_STATUS_WIFI_NOT_CONNECTED;
    set_led(true, false, false, false, false);
    esp_wifi_connect();
  }
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

#define PRINTER_QUEUE_SIZE 100

static trs_io_wifi_config_t config EXT_RAM_ATTR = {};
static struct mg_connection* ws_pipe;
static QueueHandle_t prn_queue = NULL;
static int num_printer_sockets = 0;


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

static bool extract_post_param(struct mg_http_message* message,
                               const char* param,
                               const char* nvs_key,
                               size_t max_len)
{
  // SMB URL is the longest parameter
  static char buf[MAX_LEN_SMB_URL + 1] EXT_RAM_ATTR;
  static char buf2[sizeof(buf)] EXT_RAM_ATTR;

  mg_http_get_var(&message->body, param, buf, sizeof(buf));
  // In case someone tries to force a buffer overflow
  buf[max_len + 1] = '\0';
  if (!storage_has_key(nvs_key)) {
    // Param hasn't been set before
    storage_set_str(nvs_key, buf);
    return true;
  }
  size_t len2 = max_len;
  storage_get_str(nvs_key, buf2, &len2);
  if (strcmp(buf, buf2) == 0) {
    // Param hasn't changed
    return false;
  }
  storage_set_str(nvs_key, buf);
  return true;
}

static bool mongoose_handle_config(struct mg_http_message* message,
                                   char** response,
                                   const char** content_type)
{
  bool reboot = false;
  bool smb_connect = false;

  *response = "";
  *content_type = "text/plain";

  reboot |= extract_post_param(message, "ssid", WIFI_KEY_SSID, MAX_LEN_SSID);
  reboot |= extract_post_param(message, "passwd", WIFI_KEY_PASSWD, MAX_LEN_PASSWD);
  extract_post_param(message, "tz", NTP_KEY_TZ, MAX_LEN_TZ);
  set_timezone();

  smb_connect |= extract_post_param(message, "smb_url", SMB_KEY_URL, MAX_LEN_SMB_URL);
  smb_connect |= extract_post_param(message, "smb_user", SMB_KEY_USER, MAX_LEN_SMB_USER);
  smb_connect |= extract_post_param(message, "smb_passwd", SMB_KEY_PASSWD, MAX_LEN_SMB_PASSWD);

  if (smb_connect) {
    init_trs_fs_smb();
  }

  copy_config_from_nvs();

  return reboot;
}

static void mongoose_handle_status(struct mg_http_message* message,
                                   char** response,
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
  cJSON_AddStringToObject(s, "ip", get_wifi_ip());
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

  const char* posix_err = get_posix_err_msg();
  if (posix_err != NULL) {
    cJSON_AddStringToObject(s, "posix_err", posix_err);
  }

  cJSON_AddBoolToObject(s, "has_sd_card", trs_fs_has_sd_card_reader());

  resp = cJSON_PrintUnformatted(s);
  *response = resp;
  cJSON_Delete(s);

  *content_type = "application/json";
}

static void mongoose_event_handler(struct mg_connection *c,
                                   int event, void *eventData, void *fn_data)
{
  static bool reboot = false;

  // Return if the web debugger is handling the request.
  if (trx_handle_http_request(c, event, eventData, fn_data)) {
    return;
  }

  switch (event) {
  case MG_EV_HTTP_MSG:
    {
      struct mg_http_message* message = (struct mg_http_message*) eventData;
      char* response = (char*) index_html_start;
      int response_len = strlen(response);
      const char* content_type = "text/html";

      if (mg_http_match_uri(message, "/config")) {
        reboot = mongoose_handle_config(message, &response, &content_type);
        response_len = strlen(response);
      }

      if (mg_http_match_uri(message, "/status")) {
        mongoose_handle_status(message, &response, &content_type);
        response_len = strlen(response);
      }

      if (mg_http_match_uri(message, "/printer")) {
        response = (char*) printer_html_start;
        response_len = strlen(response);
      }

      if (mg_http_match_uri(message, "/font.ttf")) {
        response = (char*) font_ttf_start;
        response_len = font_ttf_end - font_ttf_start;
        content_type = "font/ttf";
      }

      if (mg_http_match_uri(message, "/log")) {
        num_printer_sockets++;
        mg_ws_upgrade(c, message, NULL);
        c->label[0] = 'S';  // Label this connection as a web socket
        return;
      }

      mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", content_type, response_len);
      mg_send(c, response, response_len);
    }
    break;
  case MG_EV_CLOSE:
    {
      if (c->label[0] == 'S') {
        num_printer_sockets--;
      }
    }
    break;
  case MG_EV_POLL:
    {
      if (reboot) {
        esp_restart();
      }
    }
    break;
  }
}

// Pipe event handler
static void pcb(struct mg_connection* c, int ev, void* ev_data, void *fn_data) {
  if (ev == MG_EV_READ) {
    char ch;
    while (xQueueReceive(prn_queue, &ch, 0)) {
      const char* p = charToUTF8(ch);
      struct mg_connection* t;

      for (t = c->mgr->conns; t != NULL; t = t->next) {
        if (t->label[0] != 'S') continue;  // Ignore non-websocket connections
        mg_ws_send(t, p, strlen(p), WEBSOCKET_OP_TEXT);
      }
    }
  }
}

void trs_printer_write(const char ch)
{
  if (num_printer_sockets != 0) {
    xQueueSend(prn_queue, &ch, portMAX_DELAY);
    mg_mgr_wakeup(ws_pipe, NULL, 0);
  }
}

uint8_t trs_printer_read()
{
  return (num_printer_sockets == 0) ? 0xff : 0x30; /* printer selected, ready, with paper, not busy */;
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

  prn_queue = xQueueCreate(PRINTER_QUEUE_SIZE, sizeof(char));

  // Start Mongoose
  mg_mgr_init(&mgr);
  ws_pipe = mg_mkpipe(&mgr, pcb, NULL);
  mg_http_listen(&mgr, "http://0.0.0.0:80", mongoose_event_handler, &mgr);

  while(true) {
    vTaskDelay(1);
    mg_mgr_poll(&mgr, 1000);
  }
}

//--------------------------------------------------------------------------

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

  size_t len = sizeof(wifi_config.sta.ssid);
  storage_get_str(WIFI_KEY_SSID, (char*) wifi_config.sta.ssid, &len);
  len = sizeof(wifi_config.sta.password);
  storage_get_str(WIFI_KEY_PASSWD, (char*) wifi_config.sta.password, &len);

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
  if (storage_has_key(WIFI_KEY_SSID) && storage_has_key(WIFI_KEY_PASSWD)) {
    wifi_init_sta();
  } else {
    status = RS_STATUS_WIFI_NOT_CONFIGURED;
    set_led(false, true, true, true, false);
    wifi_init_ap();
  }
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(8));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

  xTaskCreatePinnedToCore(mg_task, "mg", 6000, NULL, 1, NULL, 0);
}
