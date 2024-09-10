
#include <dirent.h>
#include <sys/stat.h>
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
#include "spiffs.h"
#include "smb.h"
#include "ota.h"
#include "led.h"
#include "io.h"
#include "settings.h"
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

#define TAG "HTTP"
#define MDNS_NAME "trs-io"
#define PRINTER_QUEUE_SIZE 100

static mg_queue prn_queue;
static int num_printer_sockets = 0;

typedef void (*settings_set_t)(const string&);
typedef string& (*settings_get_t)();

static void set_screen_color(const string& color)
{
  uint8_t new_color = stoi(color);
  settings_set_screen_color(new_color);
  spi_set_screen_color(new_color);
}

static string& get_screen_color()
{
  static string color;
  color = to_string(settings_get_screen_color());
  return color;
}

#ifdef CONFIG_TRS_IO_PP
static uint8_t get_config()
{
  static uint8_t config = 0;
  static bool is_init = false;

  if (!is_init) {
    // Get DIP switch setting
    config = spi_get_config() & 0x0f;
    is_init = true;
  }
  return config;
}

static void mongoose_handle_get_roms(char** response,
                                     const char** content_type)
{
  DIR *d;
  struct dirent *dir;
  struct stat st;
  static const char *ROM_DIR = "/roms";

  *response = NULL;

  cJSON* roms = cJSON_CreateArray();
  // TODO maybe use the_fs:
  d = opendir(ROM_DIR);
  if (d == NULL) {
      ESP_LOGI(TAG, "can't open dir %s (%s)", ROM_DIR, strerror(errno));
      return;
  }
  while ((dir = readdir(d)) != NULL) {
      string pathname = string(ROM_DIR) + "/" + dir->d_name;
      printf("ROM: %s\n", pathname.c_str());
      int result = stat(pathname.c_str(), &st);
      if (result == -1) {
          ESP_LOGI(TAG, "can't stat %s (%s)", pathname.c_str(), strerror(errno));
          return;
      }
      cJSON* rom = cJSON_CreateObject();
      cJSON_AddStringToObject(rom, "filename", dir->d_name);
      cJSON_AddNumberToObject(rom, "size", st.st_size);
      cJSON_AddNumberToObject(rom, "createdAt", st.st_mtim.tv_sec);
      cJSON_AddItemToArray(roms, rom);
  }
  closedir(d);

  cJSON* selected = cJSON_CreateArray();
  for (string &romFilename : settings_get_roms()) {
      cJSON_AddItemToArray(selected, cJSON_CreateString(romFilename.c_str()));
  }

  cJSON* data = cJSON_CreateObject();
  cJSON_AddItemToObject(data, "roms", roms);
  cJSON_AddItemToObject(data, "selected", selected);

  *response = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  /*
  int i;
  uint8_t rom_selected[SETTINGS_MAX_ROMS];
  vector<FSFile>& rom_files = the_fs->getFileList();

  for (i = 0; i < SETTINGS_MAX_ROMS; i++) {
    rom_selected[i] = 0xff;
  }

  vector<string>& current_roms = settings_get_roms();
  cJSON* roms = cJSON_CreateArray();

  for (i = 0; i < rom_files.size(); i++) {
    const char* rom_file = rom_files.at(i).name.c_str();
    cJSON* rom = cJSON_CreateString(rom_file);
    cJSON_AddItemToArray(roms, rom);

    for (int j = 0; j < SETTINGS_MAX_ROMS; j++) {
      if (j == 1) {
        // Skip Model 2
        continue;
      }
      if (rom_selected[j] == 0xff && strcmp(current_roms.at(j).c_str(), rom_file) == 0) {
        rom_selected[j] = i;
        break;
      }
    }
  }

  cJSON* rom_arr = cJSON_CreateArray();
  for (i = 0; i < SETTINGS_MAX_ROMS; i++) {
    cJSON* sel = cJSON_CreateNumber(rom_selected[i] == 0xff ? 0 : rom_selected[i]);
    cJSON_AddItemToArray(rom_arr, sel);
  }

  cJSON* rom_def = cJSON_CreateObject();
  cJSON_AddItemToObject(rom_def, "roms", roms);
  cJSON_AddItemToObject(rom_def, "selected", rom_arr);

  *response = cJSON_PrintUnformatted(rom_def);
  cJSON_Delete(rom_def);
  */

  *content_type = "application/json";
}

static bool extract_rom_param(struct mg_http_message* message,
                              const char* param,
                              int model)
{
  static char buf[4] EXT_RAM_ATTR;

  if (mg_http_get_var(&message->body, param, buf, sizeof(buf)) < 0) {
    // Could not decode parameter
    return false;
  }

  vector<FSFile>& rom_files = the_fs->getFileList();
  // In case someone tries to force a buffer overflow
  buf[sizeof(buf) - 1] = '\0';
  int new_rom_idx = atoi(buf);
  string& new_rom = rom_files.at(new_rom_idx).name;

  string& orig = settings_get_rom(model);
  if (strcmp(orig.c_str(), new_rom.c_str()) == 0) {
    // Setting has not changed
    return false;
  }
  settings_set_rom(model, new_rom);
  return true;
}
#endif

static bool extract_post_param(cJSON *json,
                               const char* param,
                               settings_set_t set,
                               settings_get_t get)
{
  cJSON *value = cJSON_GetObjectItemCaseSensitive(json, param);
  if (!cJSON_IsString(value) || value->valuestring == NULL) {
      // Value is missing, ignore it.
      return false;
  }

  string& orig = get();
  if (strcmp(orig.c_str(), value->valuestring) == 0) {
    // Setting has not changed
    return false;
  }
  set(string(value->valuestring));
  return true;
}

static void mongoose_handle_status(char** response,
                                   const char** content_type);

static bool mongoose_handle_config(struct mg_http_message* message,
                                   char** response,
                                   const char** content_type)
{
  bool reboot = false;
  bool smb_connect = false;

  cJSON *json = cJSON_Parse(message->body.ptr);

  reboot |= extract_post_param(json, "ssid", settings_set_wifi_ssid, settings_get_wifi_ssid);
  reboot |= extract_post_param(json, "passwd", settings_set_wifi_passwd, settings_get_wifi_passwd);
  extract_post_param(json, "tz", settings_set_tz, settings_get_tz);
  set_timezone();

  smb_connect |= extract_post_param(json, "smb_url", settings_set_smb_url, settings_get_smb_url);
  smb_connect |= extract_post_param(json, "smb_user", settings_set_smb_user, settings_get_smb_user);
  smb_connect |= extract_post_param(json, "smb_passwd", settings_set_smb_passwd, settings_get_smb_passwd);

  extract_post_param(json, "color", set_screen_color, get_screen_color);
#ifdef CONFIG_TRS_IO_PP
  // XXX fix.
  extract_rom_param(message, "rom_m1", SETTINGS_ROM_M1);
  extract_rom_param(message, "rom_m3", SETTINGS_ROM_M3);
  extract_rom_param(message, "rom_m4", SETTINGS_ROM_M4);
  extract_rom_param(message, "rom_m4p", SETTINGS_ROM_M4P);
#endif

  settings_commit();

  if (smb_connect) {
    init_trs_fs_smb();
  }

  cJSON_Delete(json);

  // Respond with the new status so that the UI has it right away.
  mongoose_handle_status(response, content_type);

  return reboot;
}

static void mongoose_handle_status(char** response,
                                   const char** content_type)
{
  cJSON* s = cJSON_CreateObject();
  cJSON_AddNumberToObject(s, "hardware_rev", TRS_IO_HARDWARE_REVISION);
  cJSON_AddNumberToObject(s, "vers_major", TRS_IO_VERSION_MAJOR);
  cJSON_AddNumberToObject(s, "vers_minor", TRS_IO_VERSION_MINOR);
  cJSON_AddNumberToObject(s, "wifi_status", *get_wifi_status());
  cJSON_AddStringToObject(s, "ip", get_wifi_ip());
#ifdef CONFIG_TRS_IO_PP
  cJSON_AddNumberToObject(s, "config", get_config());
#endif

  uint8_t color = settings_get_screen_color();
  string& tz = settings_get_tz();
  string& ssid = settings_get_wifi_ssid();
  string& passwd = settings_get_wifi_passwd();
  string& smb_url = settings_get_smb_url();
  string& smb_user = settings_get_smb_user();
  string& smb_passwd = settings_get_smb_passwd();

  cJSON_AddNumberToObject(s, "color", color);
  if (!ssid.empty()) {
    cJSON_AddStringToObject(s, "ssid", ssid.c_str());
  }
  if (!passwd.empty()) {
    cJSON_AddStringToObject(s, "passwd", passwd.c_str());
  }
  if (!tz.empty()) {
    cJSON_AddStringToObject(s, "tz", tz.c_str());
  }
  if (!smb_url.empty()) {
    cJSON_AddStringToObject(s, "smb_url", smb_url.c_str());
  }
  if (!smb_user.empty()) {
    cJSON_AddStringToObject(s, "smb_user", smb_user.c_str());
  }
  if (!smb_passwd.empty()) {
    cJSON_AddStringToObject(s, "smb_passwd", smb_passwd.c_str());
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

  const char* frehd_msg = get_frehd_msg();
  if (frehd_msg != NULL) {
    cJSON_AddStringToObject(s, "frehd_loaded", frehd_msg);
  }

  *response = cJSON_PrintUnformatted(s);
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
      ESP_LOGI(TAG, "request %.*s %.*s",
              message->method.len,
              message->method.ptr,
              message->uri.len,
              message->uri.ptr);
      char* response = NULL; // Always allocated.
      const char* content_type = "text/html"; // Never allocated.

      if (mg_http_match_uri(message, "/config")) {
        reboot = mongoose_handle_config(message, &response, &content_type);
        if (response == NULL) {
            mg_printf(c, "HTTP/1.1 400 OK\r\nConnection: close\r\n\r\n");
            return;
        }

      } else if (mg_http_match_uri(message, "/status")) {
        mongoose_handle_status(&response, &content_type);
      } else if (mg_http_match_uri(message, "/log")) {
        num_printer_sockets++;
        mg_ws_upgrade(c, message, NULL);
        return;
#ifdef CONFIG_TRS_IO_PP
      } else if (mg_http_match_uri(message, "/get-roms")) {
        mongoose_handle_get_roms(&response, &content_type);
        if (response == NULL) {
            mg_printf(c, "HTTP/1.1 500 OK\r\nConnection: close\r\n\r\n");
            return;
        }
#endif
      }

      if (response != NULL) {
        int response_len = strlen(response);
        mg_printf(c, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", content_type, response_len);
        mg_send(c, response, response_len);
        free(response);
#ifdef CONFIG_TRS_IO_PP
      } else if (mg_http_match_uri(message, "/roms/php/connector.minimal.php")) {
        process_file_browser_req(c, message);
      } else if (strncmp(message->uri.ptr, "/roms", 5) == 0) {
        static const struct mg_http_serve_opts opts = {.root_dir = "/elFinder"};
        mg_http_serve_dir(c, (struct mg_http_message*) eventData, &opts);
#endif
      } else {
        static const struct mg_http_serve_opts opts = {.root_dir = "/html"};
        mg_http_serve_dir(c, (struct mg_http_message*) eventData, &opts);
      }
    }
    break;
  case MG_EV_CLOSE:
    {
      if (c->is_websocket) {
        num_printer_sockets--;
      }
    }
    break;
  case MG_EV_POLL:
    {
      if (reboot) {
        esp_restart();
      }

      char *ptr;
      size_t len;
      while ((len = mg_queue_next(&prn_queue, &ptr)) > 0) {
        assert(len == 1);
        const char* p = charToUTF8(*ptr);
        for (struct mg_connection* t = c->mgr->conns; t != NULL; t = t->next) {
          if (!t->is_websocket) continue;  // Ignore non-websocket connections
          mg_ws_send(t, p, strlen(p), WEBSOCKET_OP_TEXT);
        }
        mg_queue_del(&prn_queue, 1);
      }
    }
    break;
  }
}

void trs_printer_write(const char ch)
{
  if (num_printer_sockets != 0) {
    char *ptr;
    if (mg_queue_book(&prn_queue, &ptr, 1) < 1) {
      // Not enough space
    } else {
      *ptr = ch;
      mg_queue_add(&prn_queue, 1);
    }
  }
}

uint8_t trs_printer_read()
{
  return (num_printer_sockets == 0) ? 0xfe : (mg_queue_book(&prn_queue, NULL, 1) < 1) ? 0xbe : 0x3e; /* printer selected, ready, with paper, not busy */;
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
  static char printer_buf[100];

  evt_wait(EVT_START_MG);
  ESP_LOGI(TAG, "Starting Mongoose");
  init_time();
  init_mdns();

  // Start Mongoose
  // mg_log_set(MG_LL_DEBUG);
  mg_mgr_init(&mgr);
  mg_queue_init(&prn_queue, printer_buf, sizeof(printer_buf));
  mg_http_listen(&mgr, "http://0.0.0.0:80", mongoose_event_handler, &mgr);

  while(true) {
    vTaskDelay(1);
    mg_mgr_poll(&mgr, 1000);
  }
}

//--------------------------------------------------------------------------

void init_http()
{
  ESP_ERROR_CHECK(init_spiffs());

  xTaskCreatePinnedToCore(mg_task, "mg", 6000, NULL, 1, NULL, 0);
}
