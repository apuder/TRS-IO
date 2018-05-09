
#pragma once

#include "esp_system.h"

void init_io();
uint8_t read_byte();
void write_bytes(uint8_t* data, uint16_t len);
