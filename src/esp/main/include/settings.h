
#pragma once

#include <string>
#include <inttypes.h>
#include <vector>

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

#define SETTINGS_MAX_ROMS 5

#define SETTINGS_ROM_M1  0
#define SETTINGS_ROM_M3  2
#define SETTINGS_ROM_M4  3
#define SETTINGS_ROM_M4P 4

void settings_set_rom(int model, const string& rom);
string& settings_get_rom(int model);
vector<string>& settings_get_roms();
void settings_set_roms();
void settings_rename_rom(const string &oldFilename, const string &newFilename);


//-----Screen Color-----------------------------------------------
void settings_set_screen_color(uint8_t color);
uint8_t settings_get_screen_color();


//-----Printer enable-----------------------------------------------
void settings_set_printer_en(bool enable);
bool settings_get_printer_en();


//-----Keyboard Layout------------------------------------------------------

void settings_set_keyb_layout(uint8_t layout);
uint8_t settings_get_keyb_layout();

//-----Firmware updated-------------------------------------------------------

void settings_set_update_flag(bool yes);
bool settings_get_update_flag();


void settings_reset_all();
void settings_commit();
void init_settings();
