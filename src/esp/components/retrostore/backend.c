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

#define SIZE_APP_PAGE 16

typedef struct {
  char id[37];
  char title[64];
} app_title_t;

static app_title_t apps[SIZE_APP_PAGE];
static int current_page = 0;
static int num_apps  = 0;

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

static bool server_http(const char* path, const char* body,
                        bool (*parse_callback)())
{
  if (!connect_server(RETROSTORE_HOST, RETROSTORE_PORT)) {
    LOG("ERROR: Connection failed");
    return false;
  }

  snprintf(header, sizeof(header), "POST %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Content-type: application/x-www-form-urlencoded\r\n"
           "Content-Length: %d\r\n"
           "Connection: close\r\n\r\n", path, RETROSTORE_HOST,
           (int) strlen(body));
  write(fd, header, strlen(header));
  write(fd, body, strlen(body));
  
  if (!skip_to_body()) {
    return false;
  }

  bool status = parse_callback();

  close(fd);
  return status;
}

static bool pb_read_callback(pb_istream_t* stream, uint8_t* buf, size_t count)
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
  pb_istream_t stream = {&pb_read_callback, (void*)(intptr_t)fd, SIZE_MAX, NULL};
  return stream;
}

static bool pb_app_callback(pb_istream_t* stream, const pb_field_t* field,
                            void** arg)
{
  App app = {};
  
  if (!pb_decode(stream, App_fields, &app)) {
    return false;
  }

  app_title_t* app_title = &apps[num_apps];

  snprintf(app_title->title, sizeof(((app_title_t*) 0)->title), "%s (%s)",
           app.name, app.author);
  strcpy(app_title->id, app.id);

#if 0
  printf("Name: %s (%s)\n", app.name, app.id);
  printf("Author: %s\n", app.author);
  printf("Release year: %d\n", app.release_year);
  printf("Model: %d\n", app.ext_trs80.model);
  printf("Description: %s\n", app.description);
  printf("-------------------------------------------------\n");
#endif
  
  num_apps++;

  return true;
}

bool pb_list_apps_callback()
{
  ApiResponseApps response = ApiResponseApps_init_zero;
  response.app.funcs.decode = &pb_app_callback;
  pb_istream_t stream = pb_istream_from_socket();
  bool status = pb_decode(&stream, ApiResponseApps_fields, &response);
  return status && response.success;
}

static bool list_apps(const int page)
{
  snprintf(body, sizeof(body), "{\"start\":%d,\"num\":%d}",
           page * SIZE_APP_PAGE, SIZE_APP_PAGE);

  current_page = page;
  num_apps = 0;

  return server_http("/api/listApps", body, pb_list_apps_callback);
}

static pb_byte_t* cmd_bytes = NULL;
static size_t cmd_size;

static bool pb_data_callback(pb_istream_t* stream, const pb_field_t* field,
                             void** arg)
{
  if (cmd_bytes != NULL) {
    free(cmd_bytes);
  }
  cmd_size = stream->bytes_left;
  MediaImage* image = (MediaImage*) *arg;
  cmd_bytes = image->type == 3 /* CMD */ ?
    (pb_byte_t*) malloc(cmd_size) : NULL;
  return pb_read(stream, cmd_bytes, cmd_size);
}

static bool pb_media_images_callback(pb_istream_t* stream,
                                     const pb_field_t* field,
                                     void** arg)
{
  MediaImage image = {};

  image.data.funcs.decode = pb_data_callback;
  image.data.arg = &image;
  if (!pb_decode(stream, MediaImage_fields, &image)) {
    return false;
  }
    
  return true;
}

static bool pb_fetch_media_images_callback()
{
  ApiResponseMediaImages response = ApiResponseMediaImages_init_zero;
  response.mediaImage.funcs.decode = &pb_media_images_callback;
  pb_istream_t stream = pb_istream_from_socket();
  bool status = pb_decode(&stream,
                          ApiResponseMediaImages_fields,
                          &response);
  return status && response.success;
}

static bool fetch_media_images(const char* app_id)
{
  snprintf(body, sizeof(body), "{\"appId\":%s}", app_id);
  return server_http("/api/fetchMediaImages", body,
                     pb_fetch_media_images_callback);
}

static app_title_t* get_app_from_cache(int idx)
{
  int page_nr = idx / SIZE_APP_PAGE;
  int offset = idx % SIZE_APP_PAGE;
  if ((page_nr != current_page) || (num_apps == 0)) {
    if (!list_apps(page_nr)) {
      return NULL;
    }
  }
  if (offset >= num_apps) {
    return NULL;
  }
  return &apps[offset];
}

char* get_app_title(int idx)
{
  app_title_t* app = get_app_from_cache(idx);
  if (app == NULL) {
    return "";
  }
  return app->title;
}

bool get_app_cmd(int idx, unsigned char** buf, int* size)
{
  *buf = NULL;
  app_title_t* app = get_app_from_cache(idx);
  if (app == NULL) {
    return false;
  }
  if (!fetch_media_images(app->id)) {
    return false;
  }

  *buf = cmd_bytes;
  *size = cmd_size;
  return true;
}  
