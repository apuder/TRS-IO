
#include "retrostore.h"
#include "version.h"
#include "backend.h"
#include "esp_mock.h"
#include "boot.c"
#include "rsclient.c"

#include <string.h>

#define MAX_STRING_LEN 100

static int state = RS_STATE_READY;

static const char* param_types;
static int cmd;
static char str[MAX_STRING_LEN + 1];
static int str_idx;
static int b1;
static int b2;

static unsigned char* buffer;
static int left;

static void send(uint8_t* b, int len)
{
  buffer = b;
  left = len;
  state = RS_STATE_SEND;
}

static void command_send_boot(uint16_t idx, const char* dummy)
{
  send(boot_bin, boot_bin_len);
}

static void command_send_cmd(uint16_t idx, const char* dummy)
{
  if (idx == 0xffff) {
    send(rsclient_cmd, rsclient_cmd_len);
  } else {
    unsigned char* buf;
    int size;
    if (get_app_cmd(idx, &buf, &size)) {
      send(buf, size);
    } else {
      // Error happened. Just send rsclient again so we send something legal
      send(rsclient_cmd, rsclient_cmd_len);
    }      
  }
}

static void command_send_app_title(uint16_t idx, const char* dummy)
{
  char* title = get_app_title(idx);
  send((uint8_t*) title, strlen(title) + 1);
}

static void command_send_app_details(uint16_t idx, const char* dummy)
{
  char* details = get_app_details(idx);
  send((uint8_t*) details, strlen(details) + 1);
}

static void command_send_status(uint16_t idx, const char* dummy)
{
  send(get_wifi_status(), 1);
}

static void command_cmd_configure_wifi(uint16_t idx, const char* cred)
{
  const char* ssid = cred;
  const char* passwd = cred;
  while (*passwd != '\t') {
    passwd++;
  }
  *((char*) passwd) = '\0';
  passwd++;
  set_wifi_credentials(ssid, passwd);
  send(NULL, 0);
}

static void command_cmd_set_query(uint16_t idx, const char* query)
{
  set_query(query);
  send(NULL, 0);
}

static void command_send_version(uint16_t idx, const char* dummy)
{
  static uint8_t version[3] = {RS_RETROCARD_REVISION,
                               RS_RETROCARD_VERSION_MAJOR,
                               RS_RETROCARD_VERSION_MINOR};
  send(version, sizeof(version));
}

static void command_send_wifi_ssid(uint16_t idx, const char* dummy)
{
  const char* ssid = get_wifi_ssid();
  send((uint8_t*) ssid, strlen(ssid) + 1);
}

static void command_send_wifi_ip(uint16_t idx, const char* dummy)
{
  const char* ip = get_wifi_ip();
  send((uint8_t*) ip, strlen(ip) + 1);
}

typedef void (*proc_t)(uint16_t, const char*);

typedef struct {
  const char* param_types;
  proc_t proc;
} command_t;

static command_t commands[] = {
  {"", command_send_boot},
  {"I", command_send_cmd},
  {"I", command_send_app_title},
  {"I", command_send_app_details},
  {"", command_send_status},
  {"S", command_cmd_configure_wifi},
  {"S", command_cmd_set_query},
  {"", command_send_version},
  {"", command_send_wifi_ssid},
  {"", command_send_wifi_ip}
};

int rs_z80_out(int value)
{
  switch(state) {
  case RS_STATE_READY:
  case RS_STATE_SEND: // Discard previous send buffer
    cmd = value;
    str_idx = 0;
    param_types = commands[cmd].param_types;
    state = RS_STATE_PARSE_PARAMS;
    break;
  case RS_STATE_NEED_2B:
    b1 = value;
    state = RS_STATE_NEED_1B;
    break;
  case RS_STATE_NEED_1B:
    b2 = value;
    state = RS_STATE_PARSE_PARAMS;
    break;
  case RS_STATE_NEED_STRING:
    if (str_idx == MAX_STRING_LEN) {
      // String parameter too long. Stop reading.
      value = 0;
    }
    str[str_idx++] = value;
    if (value == 0) {
      state = RS_STATE_PARSE_PARAMS;
    }
    break;
  default:
    // Ignore
    break;
  }

  if (state == RS_STATE_PARSE_PARAMS) {
    if (*param_types == '\0') {
      commands[cmd].proc(b1 | (b2 << 8), str);
    } else if (*param_types == 'I') {
      param_types++;
      state = RS_STATE_NEED_2B;
    } else if (*param_types == 'S') {
      param_types++;
      state = RS_STATE_NEED_STRING;
    }
  }
  
  return state;
}

int rs_z80_in()
{
  unsigned char b = 0xff;
  if (state == RS_STATE_SEND) {
    b = *buffer++;
    left--;
    if (left == 0) {
      state = RS_STATE_READY;
    }
  }
  return b;
}

void rs_get_send_buffer(uint8_t** buf, int* size)
{
  if (state != RS_STATE_SEND) {
    *size = 0;
  } else {
    *buf = buffer;
    *size = left;
  }
  state = RS_STATE_READY;
}
