#include <strings.h>
#include "utils.h"
#include <strings.h>

bool connect_server(int* fd)
{
  const char* host = RETROSTORE_HOST;
  const int port = RETROSTORE_PORT;
  
  struct sockaddr_in serv_addr;
  struct hostent* server;

  
  *fd = socket(AF_INET, SOCK_STREAM, 0);
  if (*fd < 0) {
    // Error opening socket
    return false;
  }
  server = gethostbyname(host);
  if (server == NULL) {
    // No such host
    return false;
  }
  bzero((char*) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char*) server->h_addr,
        (char*) &serv_addr.sin_addr.s_addr,
        server->h_length);
  serv_addr.sin_port = htons(port);
  if (connect(*fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    // Error connecting
    return false;
  }
  return true;
}
  
bool skip_to_body(int fd)
{
  char buf[3];
  while (true) {
    int result = recv(fd, buf, 1, MSG_WAITALL);
    if (result < 1) {
      return false;
    }
    if (buf[0] != '\r') {
      continue;
    }
    result = recv(fd, buf, 3, MSG_WAITALL);
    if (result < 3) {
      return false;
    }
    if (buf[0] == '\n' && buf[1] == '\r' && buf[2] == '\n') {
      break;
    }
  }
  return true;
}
