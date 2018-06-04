
#pragma once

#ifdef ESP_PLATFORM
#include "esp_system.h"
#endif

#include <stddef.h>
#include <stdbool.h>

char* get_app_title(int idx);
char* get_app_description(int idx);
bool get_app_cmd(int idx, unsigned char** buf, int* size);
