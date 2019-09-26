
#pragma once

#include "esp_system.h"

void init_storage();
void storage_erase();
bool storage_has_key(const char* key, size_t* len);
bool storage_has_key(const char* key);
void storage_get_str(const char* key, char* out, size_t* len);
void storage_set_str(const char* key, const char* value);
int32_t storage_get_i32(const char* key);
void storage_set_i32(const char* key, int value);
