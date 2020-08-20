
#pragma once

//#define TRS_IO_BUTTON_ONLY_AT_STARTUP 1

#include "esp_system.h"

void init_button();
bool is_button_long_press();
bool is_button_short_press();
bool is_button_pressed();

