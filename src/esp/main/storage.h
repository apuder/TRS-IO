
#pragma once

#include "esp_system.h"

void init_storage();
void storage_erase();
bool storage_has_key(const char* key);
void storage_get(const char* key, char* out, size_t len);
void storage_set(const char* key, const char* value);

