
// vim: shiftwidth=2

#include <assert.h>
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
#include "version-ptrs.h"
#include "version.h"
#include "mdns.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mongoose.h"
#include "web_debugger.h"
#include "keyb.h"
#include "ota_stateful.h"
#include <string.h>
#include "cJSON.h"
#include "xfer.h"
#include "rst.h"
#include <algorithm>

#define TAG "HTTP"
#define MDNS_NAME "trs-io"
#define PRINTER_QUEUE_SIZE 100
#define POSIX_REST_API 0

static const char * const ROM_DIR = "/roms";
static const char * const FILES_DIR = "/";
static mg_queue prn_queue;
static int num_printer_sockets = 0;

typedef void (*settings_set_t)(const string&);
typedef string& (*settings_get_t)();

// In build/esp-idf/main/version.cpp:
extern const char* GIT_REV;
extern const char* GIT_TAG;
extern const char* GIT_BRANCH;

/**
 * We store this in the mg_connection->data object, which only has 32 bytes.
 */
struct connection_data {
  // Number of bytes expected in the upload.
  size_t expected;
  // Number of bytes received so far in the upload.
  size_t received;
  // Context for extracting an OTA tar file.
  struct extract_tar_context *ctx;
  // Whether to reboot when this connection closes.
  bool reboot_on_close;
};

// Utility class to auto-delete the JSON pointer when it goes out of scope.
class AutoDeleteJson {
    cJSON *json;

public:
    AutoDeleteJson(cJSON *json): json(json) {}
    ~AutoDeleteJson() {
        cJSON_Delete(json);
    }
};

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

static void set_printer_en(const string& printer_en)
{
  bool enable = (printer_en == "1");
  settings_set_printer_en(enable);
  spi_set_printer_en(enable);
}

static string& get_printer_en()
{
  static string printer_en;
  printer_en = settings_get_printer_en() ? "1" : "0";
  return printer_en;
}

static void set_audio_output(const string& audio_output)
{
  uint8_t new_audio_output = stoi(audio_output);
  settings_set_audio_output(new_audio_output);
  spi_set_audio_output(new_audio_output);
}

static string& get_audio_output()
{
  static string audio_output;
  audio_output = to_string(settings_get_audio_output());
  return audio_output;
}

static void set_keyboard_layout(const string& keyboard_layout)
{
  uint8_t new_keyboard_layout = stoi(keyboard_layout);
  settings_set_keyb_layout(new_keyboard_layout);
}

static string& get_keyboard_layout()
{
  static string keyboard_layout;
  keyboard_layout = to_string(settings_get_keyb_layout());
  return keyboard_layout;
}

static string makeDosFilename(const char *filename) {
    string dosFilename(filename);
    // TODO also check length etc.
    std::replace(dosFilename.begin(), dosFilename.end(), '.', '/');
    return dosFilename;
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

static string makeRomPathname(const char *filename) {
    return string(ROM_DIR) + "/" + filename;
}

#if POSIX_REST_API
static string makeFilesPathname(const char *filename) {
    if (strcmp(FILES_DIR, "/") == 0) {
        return string(FILES_DIR) + filename;
    } else {
        return string(FILES_DIR) + "/" + filename;
    }
}
#endif

static void mongoose_handle_get_roms(char** response,
                                     const char** content_type)
{
  DIR *d;
  struct dirent *dir;
  struct stat st;

  *response = NULL;

  cJSON* roms = cJSON_CreateArray();
  d = opendir(ROM_DIR);
  if (d == NULL) {
      ESP_LOGW(TAG, "can't open dir %s (%s)", ROM_DIR, strerror(errno));
      return;
  }
  while ((dir = readdir(d)) != NULL) {
      string pathname = makeRomPathname(dir->d_name);
      int result = stat(pathname.c_str(), &st);
      if (result == -1) {
          ESP_LOGW(TAG, "can't stat %s (%s)", pathname.c_str(), strerror(errno));
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
  AutoDeleteJson autoDeleteJson(data);
  cJSON_AddItemToObject(data, "roms", roms);
  cJSON_AddItemToObject(data, "selected", selected);

  *response = cJSON_PrintUnformatted(data);
  *content_type = "application/json";
}

static void mongoose_handle_roms(struct mg_http_message* message,
                                 char** response,
                                 const char** content_type)
{
    // Default to error.
    *response = NULL;

    if (mg_strcasecmp(message->method, mg_str("POST")) == 0) {
        cJSON *json = cJSON_Parse(message->body.buf);
        if (json == NULL) {
            ESP_LOGW(TAG, "Can't parse ROM JSON: %.*s", message->body.len, message->body.buf);
            return;
        }
        AutoDeleteJson autoDeleteJson(json);

        // Fetch command.
        cJSON *command = cJSON_GetObjectItemCaseSensitive(json, "command");
        if (!cJSON_IsString(command) || command->valuestring == NULL) {
            // Command is missing, ignore request.
            ESP_LOGW(TAG, "Missing ROM command");
            return;
        }

        if (strcmp(command->valuestring, "uploadRom") == 0) {
            cJSON *filename = cJSON_GetObjectItemCaseSensitive(json, "filename");
            cJSON *contents64 = cJSON_GetObjectItemCaseSensitive(json, "contents");
            if (!cJSON_IsString(filename) || filename->valuestring == NULL ||
                    !cJSON_IsString(contents64) || contents64->valuestring == NULL) {

                ESP_LOGW(TAG, "Missing ROM upload filename or contents");
                return;
            }
            // Decode base64.
            int len64 = strlen(contents64->valuestring);
            int maxLen = len64/4*3 + 1;
            vector<unsigned char> contents(maxLen);
            int actualLen = mg_base64_decode(contents64->valuestring, len64, (char *) &contents.front(), maxLen);
            string pathname = makeRomPathname(filename->valuestring);
            FILE *f = fopen(pathname.c_str(), "wb");
            if (f == NULL) {
                ESP_LOGW(TAG, "Can't write to ROM file \"%s\" (%s)", pathname.c_str(),
                        strerror(errno));
                return;
            }
            fwrite(&contents.front(), 1, actualLen, f);
            fclose(f);
        } else if (strcmp(command->valuestring, "deleteRom") == 0) {
            cJSON *filename = cJSON_GetObjectItemCaseSensitive(json, "filename");
            if (!cJSON_IsString(filename) || filename->valuestring == NULL) {
                ESP_LOGW(TAG, "Missing ROM delete filename");
                return;
            }
            string pathname = makeRomPathname(filename->valuestring);
            int result = unlink(pathname.c_str());
            if (result == -1) {
                ESP_LOGW(TAG, "can't delete ROM %s (%s)", pathname.c_str(), strerror(errno));
                return;
            }
        } else if (strcmp(command->valuestring, "renameRom") == 0) {
            cJSON *oldFilename = cJSON_GetObjectItemCaseSensitive(json, "oldFilename");
            cJSON *newFilename = cJSON_GetObjectItemCaseSensitive(json, "newFilename");
            if (!cJSON_IsString(oldFilename) || oldFilename->valuestring == NULL ||
                    !cJSON_IsString(newFilename) || newFilename->valuestring == NULL) {

                ESP_LOGW(TAG, "Missing ROM rename filenames");
                return;
            }
            string oldPathname = makeRomPathname(oldFilename->valuestring);
            string newPathname = makeRomPathname(newFilename->valuestring);
            int result = rename(oldPathname.c_str(), newPathname.c_str());
            if (result == -1) {
                ESP_LOGW(TAG, "can't rename ROM %s to %s (%s)",
                        oldPathname.c_str(), newPathname.c_str(), strerror(errno));
                return;
            }
            settings_rename_rom(oldFilename->valuestring, newFilename->valuestring);
        } else if (strcmp(command->valuestring, "assignRom") == 0) {
            cJSON *model = cJSON_GetObjectItemCaseSensitive(json, "model");
            cJSON *filename = cJSON_GetObjectItemCaseSensitive(json, "filename");
            if (!cJSON_IsString(filename) || filename->valuestring == NULL ||
                    !cJSON_IsNumber(model)) {

                ESP_LOGW(TAG, "Missing ROM assign model or filename");
                return;
            }
            settings_set_rom(model->valueint, filename->valuestring);
        } else {
            ESP_LOGW(TAG, "Unknown ROM command: %s", command->valuestring);
            return;
        }

        // JSON is auto-deleted.
    }

    // For both GET and POST:
    mongoose_handle_get_roms(response, content_type);
}
#endif // CONFIG_TRS_IO_PP

static void mongoose_handle_get_files(char** response,
                                      const char** content_type)
{
  *response = NULL;

  cJSON* data = cJSON_CreateObject();
  AutoDeleteJson autoDeleteJson(data);

  cJSON* files = cJSON_CreateArray();

#if POSIX_REST_API
  DIR_ dir;
  FRESULT result = f_opendir(&dir, FILES_DIR);
  if (result != FR_OK) {
      ESP_LOGW(TAG, "can't open files dir %s (%d)", FILES_DIR, (int) result);
      return;
  }
  FILINFO fileinfo;
  while ((result = f_readdir(&dir, &fileinfo)) == FR_OK && fileinfo.fname[0] != '\0') {
      cJSON* file = cJSON_CreateObject();
      cJSON_AddStringToObject(file, "filename", fileinfo.fname);
      cJSON_AddNumberToObject(file, "size", fileinfo.fsize);
      cJSON_AddNumberToObject(file, "createdAt", fileinfo.fdate);
      cJSON_AddItemToArray(files, file);
  }
#else
  dos_err_t err;
  uint16_t n;
  dir_buf_t* dir;

  bool success = xferModule.dosDIR(0, err, n, dir);
  if (!success) {
      cJSON_AddStringToObject(data, "error", "XFER/CMD is not running");
  } else if (err != NO_ERR) {
      // TODO return error
      printf("dosDIR err: %d\n", err);
  } else {
      for(int i = 0; i < n; i++) {
          dir_t* d = &(*dir)[i];
          uint16_t size = DIR_ENTRY_SIZE(d);

          cJSON* file = cJSON_CreateObject();
          string fname(d->fname, 15);
          while (!fname.empty() && fname[fname.size() - 1] == ' ') {
              fname.erase(fname.size() - 1, 1);
          }
          cJSON_AddStringToObject(file, "filename", fname.c_str());
          cJSON_AddNumberToObject(file, "size", size);
          cJSON_AddNumberToObject(file, "createdAt", 0);
          cJSON_AddItemToArray(files, file);
      }
  }
  xferModule.consumedResult();
#endif

  cJSON_AddItemToObject(data, "files", files);

  *response = cJSON_PrintUnformatted(data);
  *content_type = "application/json";
}

static void mongoose_handle_files(struct mg_http_message* message,
                                  char** response,
                                  const char** content_type)
{
    // Default to error.
    *response = NULL;

    if (mg_strcasecmp(message->method, mg_str("POST")) == 0) {
        cJSON *json = cJSON_Parse(message->body.buf);
        if (json == NULL) {
            ESP_LOGW(TAG, "Can't parse files JSON: %.*s", message->body.len, message->body.buf);
            return;
        }
        AutoDeleteJson autoDeleteJson(json);

        // Fetch command.
        cJSON *command = cJSON_GetObjectItemCaseSensitive(json, "command");
        if (!cJSON_IsString(command) || command->valuestring == NULL) {
            // Command is missing, ignore request.
            ESP_LOGW(TAG, "Missing files command");
            return;
        }

        if (strcmp(command->valuestring, "upload") == 0) {
            cJSON *filename = cJSON_GetObjectItemCaseSensitive(json, "filename");
            cJSON *contents64 = cJSON_GetObjectItemCaseSensitive(json, "contents");
            if (!cJSON_IsString(filename) || filename->valuestring == NULL ||
                    !cJSON_IsString(contents64) || contents64->valuestring == NULL) {

                ESP_LOGW(TAG, "Missing files upload filename or contents");
                return;
            }
            // Decode base64.
            int len64 = strlen(contents64->valuestring);
            int maxLen = len64/4*3 + 1;
            vector<uint8_t> contents(maxLen);
            int actualLen = mg_base64_decode(contents64->valuestring, len64, (char *) &contents.front(), maxLen);
#if POSIX_REST_API
            string pathname = makeFilesPathname(filename->valuestring);
            FIL fp;
            FRESULT result = f_open(&fp, pathname.c_str(), FA_WRITE | FA_CREATE_ALWAYS);
            if (result != FR_OK) {
                ESP_LOGW(TAG, "Can't write to file \"%s\" (%s)",
                        pathname.c_str(), f_get_error(result));
                return;
            }
            uint8_t *buf = &contents.front();
            int bytesLeftToWrite = actualLen;
            while (bytesLeftToWrite > 0) {
                UINT bytesWritten;
                result = f_write(&fp, buf, bytesLeftToWrite, &bytesWritten);
                if (result != FR_OK) {
                    ESP_LOGW(TAG, "Can't write to file \"%s\" (%s)",
                            pathname.c_str(), f_get_error(result));
                    return;
                }
                buf += bytesWritten;
                bytesLeftToWrite -= bytesWritten;
            }
            result = f_close(&fp);
            if (result != FR_OK) {
                ESP_LOGW(TAG, "Can't close file \"%s\" (%s)",
                        pathname.c_str(), f_get_error(result));
                return;
            }
#else
            dos_err_t err;
            sector_t sector;
            string dosFilename = makeDosFilename(filename->valuestring);
            bool success = xferModule.dosINIT(err, dosFilename.c_str());
            if (!success) {
                // TODO return error.
                printf("XFER not available\n");
                return;
            }
            if (err != NO_ERR) {
                // TODO return error.
                printf("dosINIT err: %d\n", err);
                xferModule.consumedResult();
                return;
            }
            xferModule.consumedResult();

            // Write bytes to the file.
            uint8_t *buf = &contents.front();
            uint8_t sectorIndex = 0;
            for (int i = 0; i < actualLen; i++) {
                sector[sectorIndex++] = *buf++;
                if (sectorIndex == 0) {
                    // Sector is full
                    success = xferModule.dosWRITE(err, sizeof(sector_t), &sector);
                    if (!success) {
                        // TODO return error.
                        printf("XFER not available\n");
                        return;
                    }
                    if (err != NO_ERR) {
                        // TODO return error.
                        printf("dosWRITE err: %d\n", err);
                        xferModule.consumedResult();
                        return;
                    }
                    xferModule.consumedResult();
                }
            }
            success = xferModule.dosWRITE(err, sectorIndex, &sector);
            if (!success) {
                // TODO return error.
                printf("XFER not available\n");
                return;
            }
            if (err != NO_ERR) {
                // TODO return error.
                printf("dosWRITE err: %d\n", err);
                xferModule.consumedResult();
                return;
            }
            xferModule.consumedResult();
            success = xferModule.dosCLOSE(err);
            if (!success) {
                // TODO return error.
                printf("XFER not available\n");
                return;
            }
            xferModule.consumedResult();
#endif
        } else if (strcmp(command->valuestring, "delete") == 0) {
            cJSON *filename = cJSON_GetObjectItemCaseSensitive(json, "filename");
            if (!cJSON_IsString(filename) || filename->valuestring == NULL) {
                ESP_LOGW(TAG, "Missing delete filename");
                return;
            }
#if POSIX_REST_API
            string pathname = makeFilesPathname(filename->valuestring);
            FRESULT result = f_unlink(pathname.c_str());
            if (result != FR_OK) {
                ESP_LOGW(TAG, "can't delete file %s (%s)",
                        pathname.c_str(), f_get_error(result));
                return;
            }
#else
            string dosFilename = makeDosFilename(filename->valuestring);
            dos_err_t err;
            bool success = xferModule.dosREMOVE(err, dosFilename.c_str());
            if (!success) {
                // TODO return error.
                printf("XFER not available\n");
                return;
            }
            if (err != NO_ERR) {
                // TODO return error.
                printf("dosREMOVE err: %d\n", err);
                xferModule.consumedResult();
                return;
            }
            xferModule.consumedResult();
#endif
        } else if (strcmp(command->valuestring, "rename") == 0) {
            cJSON *oldFilename = cJSON_GetObjectItemCaseSensitive(json, "oldFilename");
            cJSON *newFilename = cJSON_GetObjectItemCaseSensitive(json, "newFilename");
            if (!cJSON_IsString(oldFilename) || oldFilename->valuestring == NULL ||
                    !cJSON_IsString(newFilename) || newFilename->valuestring == NULL) {

                ESP_LOGW(TAG, "Missing rename filenames");
                return;
            }
#if POSIX_REST_API
            string oldPathname = makeFilesPathname(oldFilename->valuestring);
            string newPathname = makeFilesPathname(newFilename->valuestring);
            // Rename is not yet supported.
            // FRESULT result = f_rename(oldPathname.c_str(), newPathname.c_str());
            FRESULT result = FR_INT_ERR;
            if (result != FR_OK) {
                ESP_LOGW(TAG, "can't rename %s to %s (%s)",
                        oldPathname.c_str(), newPathname.c_str(), f_get_error(result));
                return;
            }
#else
            string dosOldFilename = makeDosFilename(oldFilename->valuestring);
            string dosNewFilename = makeDosFilename(newFilename->valuestring);
            dos_err_t err;
            bool success = xferModule.dosRENAME(err, 
              dosOldFilename.c_str(), dosNewFilename.c_str());
            if (!success) {
                // TODO return error.
                printf("XFER not available\n");
                return;
            }
            if (err != NO_ERR) {
                // TODO return error.
                printf("dosRENAME err: %d\n", err);
                xferModule.consumedResult();
                return;
            }
            xferModule.consumedResult();
#endif
        } else {
            ESP_LOGW(TAG, "Unknown files command: %s", command->valuestring);
            return;
        }

        // JSON is auto-deleted.
    }

    // For both GET and POST:
    mongoose_handle_get_files(response, content_type);
}

static void download_file(struct mg_connection *c, string const &filename)
{
#if POSIX_REST_API
    // f_read...
#else
    dos_err_t err;
    string dosFilename = makeDosFilename(filename.c_str());
    bool success = xferModule.dosOPEN(err, dosFilename.c_str());
    if (!success) {
        // TODO return error.
        printf("XFER not available\n");
        return;
    }
    if (err != NO_ERR) {
        // TODO return error.
        printf("dosOPEN err: %d\n", err);
        xferModule.consumedResult();
        return;
    }
    xferModule.consumedResult();

    mg_printf(c,
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: application/octet-stream\r\n\r\n");

    uint16_t count;
    sector_t* sector_in;
    do {
        success = xferModule.dosREAD(err, count, sector_in);
        if (!success) {
            // TODO return error.
            printf("XFER not available\n");
            return;
        }
        xferModule.consumedResult();
        mg_http_write_chunk(c, (char *) *sector_in, count);
    } while(count != 0);

    success = xferModule.dosCLOSE(err);
    if (!success) {
        // TODO return error.
        printf("XFER not available\n");
        return;
    }
    xferModule.consumedResult();
#endif
}

// Return whether the setting changed.
static bool extract_post_param(cJSON *json,
                               const char* param,
                               settings_set_t set,
                               settings_get_t get)
{
  cJSON *value = cJSON_GetObjectItemCaseSensitive(json, param);

  string newValue;
  if (cJSON_IsNumber(value)) {
      newValue = to_string(value->valueint);
  } else if (!cJSON_IsString(value) || value->valuestring == NULL) {
      // Value is missing, ignore it.
      return false;
  } else {
      newValue = value->valuestring;
  }

  if (get() == newValue) {
    // Setting has not changed
    return false;
  }

  set(newValue);
  return true;
}

static void mongoose_handle_status(char** response,
                                   const char** content_type);

// Returns whether to reboot the ESP.
static bool mongoose_handle_config(struct mg_http_message* message,
                                   char** response,
                                   const char** content_type)
{
  bool reboot = false;
  bool smb_connect = false;

  *response = NULL;

  cJSON *json = cJSON_Parse(message->body.buf);
  if (json == NULL) {
      ESP_LOGW(TAG, "Can't parse config JSON: %.*s", message->body.len, message->body.buf);
      return false;
  }
  AutoDeleteJson autoDeleteJson(json);

  reboot |= extract_post_param(json, "ssid", settings_set_wifi_ssid, settings_get_wifi_ssid);
  reboot |= extract_post_param(json, "passwd", settings_set_wifi_passwd, settings_get_wifi_passwd);
  extract_post_param(json, "tz", settings_set_tz, settings_get_tz);
  set_timezone();

  smb_connect |= extract_post_param(json, "smb_url", settings_set_smb_url, settings_get_smb_url);
  smb_connect |= extract_post_param(json, "smb_user", settings_set_smb_user, settings_get_smb_user);
  smb_connect |= extract_post_param(json, "smb_passwd", settings_set_smb_passwd, settings_get_smb_passwd);

  extract_post_param(json, "color", set_screen_color, get_screen_color);
  extract_post_param(json, "printer_en", set_printer_en, get_printer_en);
  extract_post_param(json, "audio_output", set_audio_output, get_audio_output);
  bool keyboard = extract_post_param(json, "keyboard_layout", set_keyboard_layout, get_keyboard_layout);

  settings_commit();

  if (keyboard) {
      set_keyb_layout();
  }
  if (smb_connect) {
    init_trs_fs_smb();
  }

  // Respond with the new status so that the UI has it right away.
  mongoose_handle_status(response, content_type);

  return reboot;
}

static void mongoose_handle_status(char** response,
                                   const char** content_type)
{
  uint8_t fpga_version = spi_get_fpga_version();

  cJSON* s = cJSON_CreateObject();
  AutoDeleteJson autoDeleteJson(s);
  cJSON_AddNumberToObject(s, "hardware_rev", TRS_IO_HARDWARE_REVISION);
  cJSON_AddNumberToObject(s, "vers_major", TRS_IO_VERSION_MAJOR);
  cJSON_AddNumberToObject(s, "vers_minor", TRS_IO_VERSION_MINOR);
  cJSON_AddNumberToObject(s, "fpga_vers_major", fpga_version >> 4);
  cJSON_AddNumberToObject(s, "fpga_vers_minor", fpga_version & 0x0F);
  cJSON_AddNumberToObject(s, "wifi_status", *get_wifi_status());
  cJSON_AddStringToObject(s, "ip", get_wifi_ip());
#ifdef CONFIG_TRS_IO_MODEL_1
  cJSON_AddNumberToObject(s, "board", 0);
#endif
#ifdef CONFIG_TRS_IO_MODEL_3
  cJSON_AddNumberToObject(s, "board", 1);
#endif
#ifdef CONFIG_TRS_IO_PP
  cJSON_AddNumberToObject(s, "config", get_config()); // DIP switches
  cJSON_AddNumberToObject(s, "board", 2);
#ifdef CONFIG_MINI_TRS
  cJSON_AddNumberToObject(s, "mini_trs", 1);
#endif
#endif
  cJSON_AddStringToObject(s, "git_commit", GIT_REV);
  cJSON_AddStringToObject(s, "git_tag", GIT_TAG);
  cJSON_AddStringToObject(s, "git_branch", GIT_BRANCH);

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
  cJSON_AddNumberToObject(s, "keyboard_layout", settings_get_keyb_layout());
  cJSON_AddNumberToObject(s, "printer_en", settings_get_printer_en() ? 1 : 0);
  cJSON_AddNumberToObject(s, "audio_output", settings_get_audio_output());

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
  *content_type = "application/json";
}

/**
 * Handle part of an uploaded firmware file.
 */
static void handle_firmware_data(struct mg_connection *c, struct connection_data *cd)
{
  // See if this is a streaming upload.
  if (cd->expected > 0 && c->recv.len > 0) {
    // Record what we've received.
    cd->received += c->recv.len;

    // Process more of body.
    extract_tar_error error = ete_ok;
    for (int i = 0; i < c->recv.len && error == ete_ok; i++) {
      error = extract_tar_handle_byte(cd->ctx, c->recv.buf[i]);
    }

    // Delete received data.
    c->recv.len = 0;

    // See if we've gotten the whole file.
    if (cd->received >= cd->expected) {
      // Cleanup connection data.
      extract_tar_error end_error = extract_tar_end(cd->ctx);
      if (error == ete_ok && end_error != ete_ok) {
        error = end_error;
      }
      memset(cd, 0, sizeof(*cd));

      // Send response back.
      switch (error) {
        case ete_ok:
          mg_http_reply(c, 200, NULL, "OK\n");
          break;

        case ete_corrupt_tar:
          mg_http_reply(c, 400, NULL, "Corrupted TAR file\n");
          break;

        case ete_bad_esp_flash:
          mg_http_reply(c, 500, NULL, "Error writing files to flash\n");
          break;

        case ete_firmware_update_failed:
          mg_http_reply(c, 500, NULL, "Error writing to update partition\n");
          break;

        default:
          mg_http_reply(c, 500, NULL, "Unknown error\n");
          break;
      }

      // Close connection when response gets sent.
      c->is_draining = 1;

      if (error == ete_ok) {
        // Mark the system as having been updated. This will force an FPGA
        // flash on the next reboot. We could omit this if we detect that the
        // tar file didn't contain the FPGA files.
        settings_set_update_flag(true);
        settings_commit();
      }
    }
  }
}

static void mongoose_event_handler(struct mg_connection *c, int event, void *eventData)
{
  static bool reboot = false;
  struct connection_data *cd = (struct connection_data *) c->data;
  static_assert(sizeof(connection_data) <= MG_DATA_SIZE, "Connection data is too large");

  // Return if the web debugger is handling the request.
  if (trx_handle_http_request(c, event, eventData)) {
    return;
  }

  // Selectively handle HTTP headers.
  if (event == MG_EV_HTTP_HDRS) {
    struct mg_http_message *hm = (struct mg_http_message *) eventData;

    // This upload is too large to fit in memory (over 7 GB), so we intercept it
    // when getting the headers, and use MG_EV_READ to stream it later.
    if (mg_strcasecmp(hm->method, mg_str("POST")) == 0 && mg_match(hm->uri, mg_str("/firmware"), NULL)) {
      // Store number of bytes we expect:
      cd->expected = hm->body.len;
      cd->received = 0;

      // Initialize the context.
      cd->ctx = (struct extract_tar_context *) malloc(sizeof(*cd->ctx));
      extract_tar_begin(cd->ctx);

      // Delete HTTP headers:
      mg_iobuf_del(&c->recv, 0, hm->head.len);

      // Silence HTTP protocol handler, we'll use MG_EV_READ:
      c->pfn = NULL;

      // Handle any data that we received after the headers.
      handle_firmware_data(c, cd);
      return;
    }
  }

  switch (event) {
  case MG_EV_HTTP_MSG:
    {
      struct mg_http_message* message = (struct mg_http_message*) eventData;
      ESP_LOGI(TAG, "request %.*s %.*s",
              message->method.len,
              message->method.buf,
              message->uri.len,
              message->uri.buf);
      char* response = NULL; // Always allocated.
      const char* content_type = "text/html"; // Never allocated.
      mg_str params[2];

      if (mg_match(message->uri, mg_str("/config"), NULL)) {
        reboot = mongoose_handle_config(message, &response, &content_type);
        if (response == NULL) {
            mg_printf(c, "HTTP/1.1 400 OK\r\nConnection: close\r\n\r\n");
            return;
        }
      } else if (mg_match(message->uri, mg_str("/status"), NULL)) {
        mongoose_handle_status(&response, &content_type);
      } else if (mg_match(message->uri, mg_str("/printer"), NULL)) {
        num_printer_sockets++;
        mg_ws_upgrade(c, message, NULL);
        return;
      } else if (mg_match(message->uri, mg_str("/reboot"), NULL)) {
        if (mg_strcasecmp(message->method, mg_str("POST")) == 0) {
          // Don't reboot right away, the response won't be sent back.
          // Mark connection as reboot-on-close and reboot then.
          cd->reboot_on_close = true;
          mg_http_reply(c, 200, NULL, "Rebooting\n");
        } else {
          mg_http_reply(c, 405, NULL, "Reboot only works with POST\n");
        }
      } else if (mg_match(message->uri, mg_str("/files/#"), params)) {
        string pathFilename(params[0].buf, params[0].len);
        if (mg_strcasecmp(message->method, mg_str("GET")) == 0 && !pathFilename.empty()) {
            // Download a file.
            download_file(c, pathFilename);
            return;
        } else {
            // Fetch directory or modify something.
            mongoose_handle_files(message, &response, &content_type);
            if (response == NULL) {
                mg_printf(c, "HTTP/1.1 500 OK\r\nConnection: close\r\n\r\n");
                return;
            }
        }
#ifdef CONFIG_TRS_IO_PP
      } else if (mg_match(message->uri, mg_str("/roms"), NULL)) {
        mongoose_handle_roms(message, &response, &content_type);
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
      if (cd->reboot_on_close) {
        reboot = true;
      }
    }
    break;
  case MG_EV_POLL:
    {
      if (reboot) {
        reboot_trs_io();
      }

      char *ptr;
      size_t len;
      while ((len = mg_queue_next(&prn_queue, &ptr)) > 0) {
        assert(len == 1);
        for (struct mg_connection* t = c->mgr->conns; t != NULL; t = t->next) {
            if (t->is_websocket) {
                mg_ws_send(t, ptr, len, WEBSOCKET_OP_BINARY);
            }
        }
        mg_queue_del(&prn_queue, 1);
      }
    }
    break;
  case MG_EV_READ:
    {
      // Handle streaming upload if appropriate.
      handle_firmware_data(c, cd);
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
  xTaskCreatePinnedToCore(mg_task, "mg", 6000, NULL, 1, NULL, 0);
}
