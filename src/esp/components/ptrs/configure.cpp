
#include "ptrs.h"
#include "settings.h"
#include "trs-lib.h"
#include "spi.h"
#include "wifi.h"
#include "fs-spiffs.h"

#define KEY_SCREEN_RGB "screen_rgb"
#define MAX_NUM_ROMS 10

static const char* screen_color_items[] = {
  "WHITE",
  "GREEN",
  "AMBER",
  NULL};

static uint8_t screen_color = 0;

static const char* rom_items[MAX_NUM_ROMS + 1];

static uint8_t rom_type = 0;

static bool show_splash_screen = true;

static bool enable_trs_io = true;


static form_item_t ptrs_form_items[] = {
  FORM_ITEM_HEADER("GENERAL:"),
  FORM_ITEM_CHECKBOX("Enable TRS-IO", &enable_trs_io, NULL),
  FORM_ITEM_SELECT("ROM", &rom_type, rom_items, NULL),
  FORM_ITEM_SELECT("Screen color", &screen_color, screen_color_items, NULL),
#if 0
  FORM_ITEM_INPUT("Timezone", config.tz, MAX_LEN_TZ, 0, NULL),
  FORM_ITEM_HEADER("WIFI:"),
  FORM_ITEM_INPUT("SSID", config.ssid, MAX_LEN_SSID, 0, NULL),
  FORM_ITEM_INPUT("Password", config.passwd, MAX_LEN_PASSWD, 0, NULL),
  FORM_ITEM_HEADER("SMB:"),
  FORM_ITEM_INPUT("URL", config.smb_url, MAX_LEN_SMB_URL, 40, NULL),
  FORM_ITEM_INPUT("User", config.smb_user, MAX_LEN_SMB_USER, 0, NULL),
  FORM_ITEM_INPUT("Password", config.smb_passwd, MAX_LEN_SMB_PASSWD, 0, NULL),
#endif
	FORM_ITEM_END
};

static form_t ptrs_form = {
	.title = "Configuration",
	.form_items = ptrs_form_items
};

void configure_ptrs_settings()
{
#if 0
  trs_io_wifi_config_t* config = get_wifi_config();
  init_form_begin(configuration_form);
  init_form_header("GENERAL:");
  init_form_checkbox("Show splash screen", &show_splash_screen);
  init_form_checkbox("Enable TRS-IO", &enable_trs_io);
  form_item_t* rom = init_form_select("ROM", &rom_type, rom_items);
  init_form_select("Screen color", &screen_color, screen_color_items);
  form_item_t* tz = init_form_input("Timezone", 0, MAX_LEN_TZ, config->tz);
  init_form_header("WIFI:");
  form_item_t* ssid = init_form_input("SSID", 0, MAX_LEN_SSID, config->ssid);
  form_item_t* passwd = init_form_input("Password", 0, MAX_LEN_PASSWD, config->passwd);
  init_form_header("SMB:");
  form_item_t* smb_url = init_form_input("URL", 40, MAX_LEN_SMB_URL, config->smb_url);
  form_item_t* smb_user = init_form_input("User", 0, MAX_LEN_SMB_USER, config->smb_user);
  form_item_t* smb_passwd = init_form_input("Password", 0, MAX_LEN_SMB_PASSWD, config->smb_passwd);
  init_form_header("");
  init_form_end(configuration_form);
#endif
  vector<FSFile>& rom_files = the_fs->getFileList();
  int i;

  for (i = 0; i < 10 && i < rom_files.size(); i++) {
    rom_items[i] = rom_files.at(i).name.c_str();
  }
  rom_items[i] = NULL;

  screen_color = settings_get_screen_color();

#if 0
  screen_color = (uint8_t) settingsScreen.getScreenColor();
  rom_type = (uint8_t) settingsROM.getROMType();
  show_splash_screen = !settingsSplashScreen.hideSplashScreen();
  enable_trs_io = settingsTrsIO.isEnabled();
#endif

  form(&ptrs_form, false);

  // Download ROM
  string new_rom = "/roms/";
  new_rom += rom_items[rom_type];
  printf("Downloading ROM: %s\n", new_rom.c_str());
  FILE* f = fopen(new_rom.c_str(), "rb");
  if (f != NULL) {
    static uint8_t buf[100];
    int br;
    uint16_t addr = 0;
    do {
      br = fread(buf, 1, sizeof(buf), f);
      for (int x = 0; x < br; x++) {
        spi_bram_poke(addr++, buf[x]);
      }
    } while (br != 0);
    fclose(f);
  }

  // Set screen color
  settings_set_screen_color(screen_color);
  spi_set_screen_color(screen_color);

#if 0
  settingsScreen.setScreenColor((screen_color_t) screen_color);
  settingsSplashScreen.hideSplashScreen(!show_splash_screen);
  settingsROM.setROMType((rom_type_t) rom_type);
#ifndef CONFIG_POCKET_TRS_TTGO_VGA32_SUPPORT
  settingsTrsIO.setEnabled(enable_trs_io);
#endif

  if (smb_url->dirty || smb_user->dirty || smb_passwd->dirty) {
    init_trs_fs_smb(config->smb_url, config->smb_user, config->smb_passwd);
  }

  if (tz->dirty) {
    set_timezone(config->tz);
  }

  if (ssid->dirty || passwd->dirty || rom->dirty) {
    wnd_popup("Rebooting PocketTRS...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    set_wifi_credentials(config->ssid, config->passwd);
  }
#endif
}
