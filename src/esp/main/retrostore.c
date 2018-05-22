#include "retrostore.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#include "ApiProtos.pb.h"
#include "pb_decode.h"

#define LOG(msg) printf("%s\n", msg)


static const char* RETROSTORE_HOST = "retrostore.org";
static const int RETROSTORE_PORT = 80;

static char header[512];
static char body[64];

static int fd;

static bool connect_server(const char* host, int port)
{
  struct sockaddr_in serv_addr;
  struct hostent* server;
  
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
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
  if (connect(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    // Error connecting
    return false;
  }
  return true;
}
  
static bool skip_to_body()
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

static bool read_callback(pb_istream_t* stream, uint8_t* buf, size_t count)
{
  int fd = (intptr_t) stream->state;
  int offset = 0;
  ssize_t size_read;
  
  while (true) {
    size_read = recv(fd, buf + offset, count, MSG_WAITALL);

    if (size_read < 0) {
      return false;
    }

    if (size_read == 0) {
      stream->bytes_left = 0; /* EOF */
      break;
    }

    if (size_read == count) {
      return true;
    }
    
    offset += size_read;
    count -= size_read;
  }

  return size_read == count;
}

static pb_istream_t pb_istream_from_socket()
{
  pb_istream_t stream = {&read_callback, (void*)(intptr_t)fd, SIZE_MAX, NULL};
  return stream;
}
  
static bool app_callback(pb_istream_t* stream, const pb_field_t* field,
                         void** arg)
{
  App app = {};

  if (!pb_decode(stream, App_fields, &app)) {
    return false;
  }
    
  printf("Name: %s\n", app.name);

  return true;
}

bool list_apps(const int start, const int num)
{
  if (!connect_server(RETROSTORE_HOST, RETROSTORE_PORT)) {
    LOG("ERROR: Connection failed");
    return false;
  }

  snprintf(body, sizeof(body), "{\"start\":%d,\"num\":%d}", start, num);
  snprintf(header, sizeof(header), "POST /api/listApps HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Content-type: application/x-www-form-urlencoded\r\n"
           "Content-Length: %d\r\n"
           "Connection: close\r\n\r\n", RETROSTORE_HOST, strlen(body));
  write(fd, header, strlen(header));
  write(fd, body, strlen(body));
  
  if (!skip_to_body()) {
    return false;
  }

  ApiResponseApps response = ApiResponseApps_init_zero;
  response.app.funcs.decode = &app_callback;
  pb_istream_t stream = pb_istream_from_socket();
  bool status = pb_decode(&stream, ApiResponseApps_fields, &response);

  if (!status) {
    LOG("ERROR: Decoding of message failed");
    return false;
  }

  if (!response.success) {
    LOG("Server reported an ERROR");
    return false;
  }

  close(fd);
  return true;
}
