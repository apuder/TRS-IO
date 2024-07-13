
#include "ptrs.h"
#include "settings.h"
#include "trs-lib.h"
#include "trs-fs.h"
#include "spi.h"
#include "wifi.h"
#include "fs-spiffs.h"
#include "ntp_sync.h"
#include "fabgl.h"
#include "keyb.h"
#include <freertos/task.h>


static const char* screen_color_items[] = {
  "WHITE",
  "GREEN",
  "AMBER",
  NULL};

static uint8_t screen_color = 0;
static bool screen_color_dirty;

static bool show_splash_screen = true;

static bool enable_trs_io = true;

static char tz[MAX_LEN_TZ + 1] EXT_RAM_ATTR;
static bool tz_dirty;

static bool wifi_dirty;
static char wifi_ssid[MAX_LEN_WIFI_SSID + 1] EXT_RAM_ATTR;
static char wifi_passwd[MAX_LEN_WIFI_PASSWD + 1] EXT_RAM_ATTR;

static bool smb_dirty;
static char smb_url[MAX_LEN_SMB_URL + 1] EXT_RAM_ATTR;
static char smb_user[MAX_LEN_SMB_USER + 1] EXT_RAM_ATTR;
static char smb_passwd[MAX_LEN_SMB_PASSWD + 1] EXT_RAM_ATTR;

static bool keyb_layout_dirty;
static uint8_t keyb_layout = 0;
static const char** keyb_name_items = NULL;

static void init_keyb_names()
{
  uint8_t i;
  uint8_t n_countries = SupportedLayouts::count();
  keyb_name_items = (const char**) malloc((n_countries + 1) * sizeof(char*));
  for (i = 0; i < n_countries; i++) {
    keyb_name_items[i] = SupportedLayouts::names()[i];
  }
  keyb_name_items[i] = NULL;
}

static form_item_t ptrs_form_items[] = {
  FORM_ITEM_HEADER("GENERAL:"),
  FORM_ITEM_CHECKBOX("Enable TRS-IO", &enable_trs_io, NULL),
  FORM_ITEM_SELECT("Screen color", &screen_color, screen_color_items, &screen_color_dirty),
  FORM_ITEM_SELECT_PTR("Keyboard", &keyb_layout, &keyb_name_items, &keyb_layout_dirty),
  FORM_ITEM_INPUT("Timezone", tz, MAX_LEN_TZ, 0, &tz_dirty),
  FORM_ITEM_HEADER("WIFI:"),
  FORM_ITEM_INPUT("SSID", wifi_ssid, MAX_LEN_WIFI_SSID, 0, &wifi_dirty),
  FORM_ITEM_INPUT("Password", wifi_passwd, MAX_LEN_WIFI_PASSWD, 0, &wifi_dirty),
  FORM_ITEM_HEADER("SMB:"),
  FORM_ITEM_INPUT("URL", smb_url, MAX_LEN_SMB_URL, 40, &smb_dirty),
  FORM_ITEM_INPUT("User", smb_user, MAX_LEN_SMB_USER, 0, &smb_dirty),
  FORM_ITEM_INPUT("Password", smb_passwd, MAX_LEN_SMB_PASSWD, 0, &smb_dirty),
	FORM_ITEM_END
};

static form_t ptrs_form = {
	.title = "Configuration",
	.form_items = ptrs_form_items
};

bool configure_ptrs_settings()
{
  bool reboot = false;

  if (keyb_name_items == NULL) {
    init_keyb_names();
  }

  string& _wifi_ssid = settings_get_wifi_ssid();
  string& _wifi_passwd = settings_get_wifi_passwd();
  string& _tz = settings_get_tz();
  string& _smb_url = settings_get_smb_url();
  string& _smb_user = settings_get_smb_user();
  string& _smb_passwd = settings_get_smb_passwd();

  strncpy(wifi_ssid, _wifi_ssid.c_str(), MAX_LEN_WIFI_SSID);
  strncpy(wifi_passwd, _wifi_passwd.c_str(), MAX_LEN_WIFI_PASSWD);
  strncpy(tz, _tz.c_str(), MAX_LEN_TZ);
  strncpy(smb_url, _smb_url.c_str(), MAX_LEN_SMB_URL);
  strncpy(smb_user, _smb_user.c_str(), MAX_LEN_SMB_USER);
  strncpy(smb_passwd, _smb_passwd.c_str(), MAX_LEN_SMB_PASSWD);

  screen_color = settings_get_screen_color();
  keyb_layout = settings_get_keyb_layout();

  form(&ptrs_form, false);

  if (screen_color_dirty) {
    // Set screen color
    settings_set_screen_color(screen_color);
    spi_set_screen_color(screen_color);
  }

  if (keyb_layout_dirty) {
    // Set keyboard layout
    settings_set_keyb_layout(keyb_layout);
    set_keyb_layout();
  }

  if (smb_dirty) {
    settings_set_smb_url(string(smb_url));
    settings_set_smb_user(string(smb_user));
    settings_set_smb_passwd(string(smb_passwd));
    init_trs_fs_smb();
  }

  if (tz_dirty) {
    settings_set_tz(string(tz));
    set_timezone();
  }

  if (wifi_dirty) {
    settings_set_wifi_ssid(wifi_ssid);
    settings_set_wifi_passwd(wifi_passwd);
    reboot = true;
  }

  settings_commit();

  return reboot;
}
