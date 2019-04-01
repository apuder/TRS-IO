
#ifndef FILEIO_SERIAL_IO_H
#define FILEIO_SERIAL_IO_H

#include <inttypes.h>

int16_t read_byte();
void read_blob(void* buf, int32_t btr);
void write_byte(uint8_t b);
void write_blob(void* buf, int32_t btw);
void init_serial_io();

#endif
