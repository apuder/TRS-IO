
#include "settings.h"
#include "nvs_flash.h"
#include <string.h>




static nvs_handle storage;

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
  size_t len;

  if (nvs_get_str(storage, NTP_KEY_TZ, NULL, &len) == ESP_OK) {
    tz.resize(len);
    nvs_get_str(storage, NTP_KEY_TZ, (char*) tz.data(), &len);
  }
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
  size_t len;

  if (nvs_get_str(storage, WIFI_KEY_SSID, NULL, &len) == ESP_OK) {
    wifi_ssid.resize(len);
    nvs_get_str(storage, WIFI_KEY_SSID, (char*) wifi_ssid.data(), &len);
  }

  if (nvs_get_str(storage, WIFI_KEY_PASSWD, NULL, &len) == ESP_OK) {
    wifi_passwd.resize(len);
    nvs_get_str(storage, WIFI_KEY_PASSWD, (char*) wifi_passwd.data(), &len);
  }
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
  size_t len;

  if (nvs_get_str(storage, SMB_KEY_URL, NULL, &len) == ESP_OK) {
    smb_url.resize(len);
    nvs_get_str(storage, SMB_KEY_URL, (char*) smb_url.data(), &len);
  }
  if (nvs_get_str(storage, SMB_KEY_USER, NULL, &len) == ESP_OK) {
    smb_user.resize(len);
    nvs_get_str(storage, SMB_KEY_USER, (char*) smb_user.data(), &len);
  }
  if (nvs_get_str(storage, SMB_KEY_PASSWD, NULL, &len) == ESP_OK) {
    smb_passwd.resize(len);
    nvs_get_str(storage, SMB_KEY_PASSWD, (char*) smb_passwd.data(), &len);
  }
}


//-----ROM--------------------------------------------------------

#define KEY_ROM_PREFIX "rom_"

static vector<string> roms;

static const char* default_roms[] = {
  "level2.bin",
  "",
  "model3-frehd.bin",
  "model3-frehd.bin",
  "model3-frehd.bin"
};

vector<string>& settings_get_roms()
{
  return roms;
}

void settings_set_roms()
{
  char key[] = KEY_ROM_PREFIX "0\0";
  for(int i = 0; i < SETTINGS_MAX_ROMS; i++) {
    key[strlen(KEY_ROM_PREFIX)] = '0' + i;
    nvs_set_str(storage, key, roms.at(i).c_str());
  }
}

static void init_rom()
{
  char key[] = KEY_ROM_PREFIX "0\0";
  size_t len;

  for(int i = 0; i < SETTINGS_MAX_ROMS; i++) {
    string rom;
    key[strlen(KEY_ROM_PREFIX)] = '0' + i;
    if (nvs_get_str(storage, key, NULL, &len) == ESP_OK) {
      rom.resize(len);
      nvs_get_str(storage, key, (char*) rom.data(), &len);
    }

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

//-------------------------------------------------------------------------

void settings_reset_all()
{
  ESP_ERROR_CHECK(nvs_erase_all(storage));
  ESP_ERROR_CHECK(nvs_commit(storage));
}

void settings_commit()
{
  nvs_commit(storage);
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
}
