
#include "settings.h"
#include "nvs_flash.h"
#include <string.h>




static nvs_handle storage;

// If storage has the key, the value is written into "value" and true is returned.
// Otherwise "value" is untouched and false is returned.
static bool get_nvs_string(char *key, string &value)
{
    size_t len;
    if (nvs_get_str(storage, key, NULL, &len) == ESP_OK) {
        // Must write to a temporary buffer because the value.data() method
        // returns a buffer that must not be modified.

        // Len includes terminating nul.
        vector<char> buffer(len);
        nvs_get_str(storage, key, &buffer.front(), &len);

        value = &buffer.front();

        return true;
    }

    return false;
}

//-----Timezone-------------------------------------------------------------

#define NTP_KEY_TZ "tz"

static string tz;


string& settings_get_tz()
{
  return tz;
}

void settings_set_tz(const string& new_tz)
{
  nvs_set_str(storage, NTP_KEY_TZ, new_tz.c_str());
  tz = new_tz;
}

static void init_tz()
{
  get_nvs_string(NTP_KEY_TZ, tz);
}



//-----WiFi-----------------------------------------------------------------

#define WIFI_KEY_SSID "ssid"
#define WIFI_KEY_PASSWD "passwd"

static string wifi_ssid;
static string wifi_passwd;


bool settings_has_wifi_credentials()
{
  return !wifi_ssid.empty() && !wifi_passwd.empty();
}

string& settings_get_wifi_ssid()
{
  return wifi_ssid;
}

string& settings_get_wifi_passwd()
{
  return wifi_passwd;
}

void settings_set_wifi_ssid(const string& ssid)
{
  nvs_set_str(storage, WIFI_KEY_SSID, ssid.c_str());
  wifi_ssid = ssid;
}

void settings_set_wifi_passwd(const string& passwd)
{
  nvs_set_str(storage, WIFI_KEY_PASSWD, passwd.c_str());
  wifi_passwd = passwd;
}

static void init_wifi_credentials()
{
  get_nvs_string(WIFI_KEY_SSID, wifi_ssid);
  get_nvs_string(WIFI_KEY_PASSWD, wifi_passwd);
}



//-----SMB Credentials------------------------------------------------------

#define SMB_KEY_URL "smb_url"
#define SMB_KEY_USER "smb_user"
#define SMB_KEY_PASSWD "smb_passwd"

static string smb_url;
static string smb_user;
static string smb_passwd;

bool settings_has_smb_credentials()
{
    return !smb_url.empty() && !smb_user.empty() && !smb_passwd.empty();
}

string& settings_get_smb_url()
{
  return smb_url;
}

string& settings_get_smb_user()
{
  return smb_user;
}

string& settings_get_smb_passwd()
{
  return smb_passwd;
}

void settings_set_smb_url(const string& url)
{
  nvs_set_str(storage, SMB_KEY_URL, url.c_str());
  smb_url = url;
}

void settings_set_smb_user(const string& user)
{
  nvs_set_str(storage, SMB_KEY_USER, user.c_str());
  smb_user = user;
}

void settings_set_smb_passwd(const string& passwd)
{
  nvs_set_str(storage, SMB_KEY_PASSWD, passwd.c_str());
  smb_passwd = passwd;
}

static void init_smb_credentials()
{
  get_nvs_string(SMB_KEY_URL, smb_url);
  get_nvs_string(SMB_KEY_USER, smb_user);
  get_nvs_string(SMB_KEY_PASSWD, smb_passwd);
}


//-----ROM--------------------------------------------------------

#define KEY_ROM_PREFIX "rom_"

static vector<string> roms;

static const char* default_roms[] = {
  "level2-frehd.bin",
  "",
  "model3-frehd.bin",
  "model3-frehd.bin",
  "model4p-frehd.bin"
};

vector<string>& settings_get_roms()
{
  return roms;
}

void settings_set_rom(int model, const string& rom)
{
  roms.at(model) = rom;  
  char key[] = KEY_ROM_PREFIX "0\0";
  key[strlen(KEY_ROM_PREFIX)] = '0' + model;
  nvs_set_str(storage, key, roms.at(model).c_str());
}

string& settings_get_rom(int model)
{
  return roms.at(model);
}

void settings_set_roms()
{
  char key[] = KEY_ROM_PREFIX "0\0";
  for(int i = 0; i < SETTINGS_MAX_ROMS; i++) {
    key[strlen(KEY_ROM_PREFIX)] = '0' + i;
    nvs_set_str(storage, key, roms.at(i).c_str());
  }
}

// Call when the file has been renamed.
void settings_rename_rom(const string &oldFilename, const string &newFilename)
{
    bool changed = false;

    for (int i = 0; i < roms.size(); i++) {
        if (roms[i] == oldFilename) {
            roms[i] = newFilename;
            changed = true;
        }
    }

    if (changed) {
        settings_set_roms();
    }
}

static void init_rom()
{
  char key[] = KEY_ROM_PREFIX "0\0";

  for(int i = 0; i < SETTINGS_MAX_ROMS; i++) {
    string rom;
    key[strlen(KEY_ROM_PREFIX)] = '0' + i;
    get_nvs_string(key, rom);

    if (rom.empty()) {
      rom = default_roms[i];
    }
    roms.push_back(rom);
  }
}


//-----Screen Color--------------------------------------------------------

#define KEY_SCREEN_RGB "screen_rgb"

static int32_t screen_color;

void settings_set_screen_color(uint8_t color)
{
  nvs_set_i32(storage, KEY_SCREEN_RGB, color);
  screen_color = color;
}

uint8_t settings_get_screen_color()
{
  return screen_color & 0xff;
}


static void init_screen_color()
{
  if (nvs_get_i32(storage, KEY_SCREEN_RGB, &screen_color) != ESP_OK) {
    screen_color = 0;
  }
}


//-----Printer enable--------------------------------------------------------

#define KEY_PRINTER_EN "printer_en"

static int8_t printer_en;

void settings_set_printer_en(bool enable)
{
  printer_en = enable ? 1 : 0;
  nvs_set_i8(storage, KEY_PRINTER_EN, printer_en);
}

bool settings_get_printer_en()
{
  return printer_en != 0;
}


static void init_printer_en()
{
  if (nvs_get_i8(storage, KEY_PRINTER_EN, &printer_en) != ESP_OK) {
    printer_en = 1;
  }
}

//-----Keyboard Layout------------------------------------------------------

#define KEY_KEYB_LAYOUT "keyb"

// Index for US Layout in fabgl
#define DEFAULT_US_LAYOUT 3

static int32_t keyb_layout;

void settings_set_keyb_layout(uint8_t layout)
{
  nvs_set_i32(storage, KEY_KEYB_LAYOUT, layout);
  keyb_layout = layout;
}

uint8_t settings_get_keyb_layout()
{
  return keyb_layout & 0xff;
}


static void init_keyb_layout()
{
  if (nvs_get_i32(storage, KEY_KEYB_LAYOUT, &keyb_layout) != ESP_OK) {
    keyb_layout = DEFAULT_US_LAYOUT; 
  }
}


//-----Firmware updated-------------------------------------------------------

#define KEY_UPDATE "update"

static int8_t update;

void settings_set_update_flag(bool yes)
{
  update = yes ? 1 : 0;
  nvs_set_i8(storage, KEY_UPDATE, update);
}

bool settings_get_update_flag()
{
  return update != 0;
}


static void init_update_flag()
{
  if (nvs_get_i8(storage, KEY_UPDATE, &update) != ESP_OK) {
    update = 0;
  }
}

//-------------------------------------------------------------------------

void settings_reset_all()
{
  ESP_ERROR_CHECK(nvs_erase_all(storage));
  ESP_ERROR_CHECK(nvs_commit(storage));
}

void settings_commit()
{
  ESP_ERROR_CHECK(nvs_commit(storage));
}

void init_settings()
{
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(nvs_open("retrostore", NVS_READWRITE, &storage));

  init_tz();
  init_wifi_credentials();
  init_smb_credentials();
  init_rom();
  init_screen_color();
  init_printer_en();
  init_keyb_layout();
  init_update_flag();
}
