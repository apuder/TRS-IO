
#pragma once

#ifdef ESP_PLATFORM
#include "esp_system.h"
#endif

#include <stddef.h>
#include <stdbool.h>

void set_query(const char* query);
char* get_app_title(int idx);
char* get_app_details(int idx);
bool get_app_code(int idx, int* type, unsigned char** buf, int* size);
void get_last_app_code(unsigned char** buf, int* size);
