
#pragma once

#include "esp_system.h"

void init_led();
void set_led(bool r, bool g, bool b, bool blink, bool auto_off);
