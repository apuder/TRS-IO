
#ifndef __UTILS_H__
#define __UTILS_H__

#include "defs.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#define RETROSTORE_HOST "retrostore.org"
#define RETROSTORE_PORT 80

#define LOG(msg) printf("%s\n", msg)

bool connect_server(int* fd);
bool skip_to_body(int fd);

#endif
