
#pragma once

//#define TRS_IO_BUTTON_ONLY_AT_STARTUP 1

#include "esp_system.h"

void init_button();
bool is_status_button_long_press();
bool is_status_button_short_press();
bool is_status_button_pressed();
#ifdef CONFIG_TRS_IO_PP
bool is_reset_button_long_press();
bool is_reset_button_short_press();
bool is_reset_button_pressed();
#endif
