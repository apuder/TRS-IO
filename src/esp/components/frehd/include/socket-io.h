
#ifndef FILEIO_SOCKET_IO_H
#define FILEIO_SOCKET_IO_H

#include <inttypes.h>

int16_t read_byte();
void read_blob(void* buf, int32_t btr);
void write_byte(uint8_t b);
void write_blob(void* buf, int32_t btw);
void init_socket_io();

#endif //FILEIO_SOCKET_IO_H
