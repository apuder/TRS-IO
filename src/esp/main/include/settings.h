
#pragma once

#include <string>
#include <inttypes.h>

using namespace std;


//-----Timezone---------------------------------------------------

#define MAX_LEN_TZ 32

string& settings_get_tz();
void settings_set_tz(const string& new_tz);

//-----WiFi-------------------------------------------------------

#define MAX_LEN_WIFI_SSID 32
#define MAX_LEN_WIFI_PASSWD 32

bool settings_has_wifi_credentials();
string& settings_get_wifi_ssid();
string& settings_get_wifi_passwd();
void settings_set_wifi_ssid(const string& ssid);
void settings_set_wifi_passwd(const string& passwd);

//-----SMB--------------------------------------------------------

#define MAX_LEN_SMB_URL 100
#define MAX_LEN_SMB_USER 32
#define MAX_LEN_SMB_PASSWD 32

bool settings_has_smb_credentials();
string& settings_get_smb_url();
string& settings_get_smb_user();
string& settings_get_smb_passwd();
void settings_set_smb_url(const string& url);
void settings_set_smb_user(const string& user);
void settings_set_smb_passwd(const string& passwd);

//-----ROM--------------------------------------------------------

#define SETTING_DEFAULT_ROM "model3-frehd.rom"

string& settings_get_rom();
void settings_set_rom(const string& rom_file);


//-----Screen Color-----------------------------------------------
void settings_set_screen_color(uint8_t color);
uint8_t settings_get_screen_color();


void settings_reset_all();
void settings_commit();
void init_settings();
