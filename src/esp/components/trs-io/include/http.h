
#pragma once

#include "defs.h"
#include <string>

using namespace std;


void trs_printer_write(const char ch);
uint8_t trs_printer_read();
void init_http();

// The firmware that embeds TRS-IO can serve requests of its own (PocketTRS:
// its Bluetooth keyboard, under /bt). The handler gets the request's method,
// URI and body. It returns the response, JSON allocated with malloc(), or
// NULL if the request is not one of its own.
typedef char* (*http_host_handler_t)(const char* method, const char* uri, const char* body);
void http_set_host_handler(http_host_handler_t handler);
