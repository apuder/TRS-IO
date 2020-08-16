#include "retrostore.h"

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

#include "ApiProtos.pb.h"
#include "pb_decode.h"
#include "cJSON.h"

#include "esp_attr.h"

static int fd;

#define SIZE_APP_PAGE 16

typedef struct {
  char id[37];
  char title[64];
} app_title_t;

static app_title_t apps[SIZE_APP_PAGE] EXT_RAM_ATTR;
static int current_page = 0;
static int num_apps  = 0;

static char* query = NULL;

void set_query(const char* query_)
{
  if (query != NULL) {
    free(query);
  }
  query = (char*) malloc(strlen(query_) + 1);
  memcpy(query, query_, strlen(query_) + 1);
  current_page = 0;
  num_apps = 0;
}

static bool server_http(const char* path, cJSON* params,
                        bool (*parse_callback)())
{
  if (!connect_server(&fd)) {
    LOG("ERROR: Connection failed");
    cJSON_Delete(params);
    return false;
  }

  char* header = (char*) malloc(512);
  char* body = cJSON_PrintUnformatted(params);
  snprintf(header, 512, "POST %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Content-type: application/x-www-form-urlencoded\r\n"
           "Content-Length: %d\r\n"
           "Connection: close\r\n\r\n", path, RETROSTORE_HOST,
           (int) strlen(body));
  write(fd, header, strlen(header));
  write(fd, body, strlen(body));
  free(header);
  free(body);
  cJSON_Delete(params);

  if (!skip_to_body(fd)) {
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

static bool pb_app_title_callback(pb_istream_t* stream, const pb_field_t* field,
                                  void** arg)
{
  App app = {};
  
  if (!pb_decode(stream, App_fields, &app)) {
    return false;
  }

  app_title_t* app_title = &apps[num_apps];

  if (snprintf(app_title->title, sizeof(((app_title_t*) 0)->title), "%s (%s)",
               app.name, app.author) < 0) {
    app_title->title[0] = '\0';
  }
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
  response.app.funcs.decode = &pb_app_title_callback;
  pb_istream_t stream = pb_istream_from_socket();
  bool status = pb_decode(&stream, ApiResponseApps_fields, &response);
  return status && response.success;
}

static bool list_apps(const int page)
{
  static const char* types[] = {"COMMAND", "BASIC"};
  
  cJSON* params = cJSON_CreateObject();
  cJSON_AddNumberToObject(params, "start", page * SIZE_APP_PAGE);
  cJSON_AddNumberToObject(params, "num", SIZE_APP_PAGE);
  cJSON* mediaTypes = cJSON_CreateStringArray(types, 2);
  cJSON* trs80 = cJSON_CreateObject();
  cJSON_AddItemToObject(trs80, "mediaTypes", mediaTypes);
  cJSON_AddItemToObject(params, "trs80", trs80);
  if (query != NULL) {
    cJSON_AddStringToObject(params, "query", query);
  }

  current_page = page;
  num_apps = 0;

  return server_http("/api/listApps", params, pb_list_apps_callback);
}

static char app_details[1024];

static bool pb_app_details_callback(pb_istream_t* stream,
                                    const pb_field_t* field,
                                    void** arg)
{
  App app = {};
  
  if (!pb_decode(stream, App_fields, &app)) {
    return false;
  }

  if (snprintf(app_details, sizeof(app_details), "%s\nAuthor: %s\n"
               "Year: %d\n\n%s", app.name,app.author, app.release_year,
               app.description) < 0) {
    app_details[0] = '\0';
  }

  return true;
}

bool pb_get_app_callback()
{
  ApiResponseApps response = ApiResponseApps_init_zero;
  response.app.funcs.decode = &pb_app_details_callback;
  pb_istream_t stream = pb_istream_from_socket();
  bool status = pb_decode(&stream, ApiResponseApps_fields, &response);
  return status && response.success;
}

static bool get_app(const char* app_id)
{
  cJSON* params = cJSON_CreateObject();
  cJSON_AddStringToObject(params, "appId", app_id);
  return server_http("/api/getApp", params, pb_get_app_callback);
}

static pb_byte_t* app_bytes = NULL;
static size_t app_size;
static int app_type = 0;

static bool pb_data_callback(pb_istream_t* stream, const pb_field_t* field,
                             void** arg)
{
    if (app_bytes != NULL) {
    free(app_bytes);
  }
  app_size = stream->bytes_left;
  MediaImage* image = (MediaImage*) *arg;
  app_type = image->type;
  app_bytes = (app_type == MediaType_COMMAND) || (app_type == MediaType_BASIC) ?
    (pb_byte_t*) malloc(app_size) : NULL;
  return pb_read(stream, app_bytes, app_size);
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
  cJSON* params = cJSON_CreateObject();
  cJSON_AddStringToObject(params, "appId", app_id);
  return server_http("/api/fetchMediaImages", params,
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

char* get_app_details(int idx)
{
  app_title_t* app = get_app_from_cache(idx);
  if (app == NULL) {
    return "";
  }
  if (!get_app(app->id)) {
    return "";
  }
  return app_details;
}

bool get_app_code(int idx, int* type, unsigned char** buf, int* size)
{
  *buf = NULL;
  app_title_t* app = get_app_from_cache(idx);
  if (app == NULL) {
    return false;
  }
  if (!fetch_media_images(app->id)) {
    return false;
  }

  *type = app_type;
  *buf = app_bytes;
  *size = app_size;
  return true;
}  

void get_last_app_code(unsigned char** buf, int* size)
{
  *buf = app_bytes;
  *size = app_size;
}  
