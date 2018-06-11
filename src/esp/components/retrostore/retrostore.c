
#include "retrostore.h"
#include "backend.h"
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

static void command_send_boot(uint16_t idx, const char* str)
{
  send(boot_bin, boot_bin_len);
}

static void command_send_cmd(uint16_t idx, const char* str)
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

static void command_send_app_title(uint16_t idx, const char* search_terms)
{
  set_search_terms(search_terms);
  char* title = get_app_title(idx);
  send((uint8_t*) title, strlen(title) + 1);
}

static void command_send_app_details(uint16_t idx, const char* search_terms)
{
  set_search_terms(search_terms);
  char* details = get_app_details(idx);
  send((uint8_t*) details, strlen(details) + 1);
}

typedef void (*proc_t)(uint16_t, const char*);

typedef struct {
  const char* param_types;
  proc_t proc;
} command_t;

static command_t commands[] = {
  {"", command_send_boot},
  {"I", command_send_cmd},
  {"IS", command_send_app_title},
  {"IS", command_send_app_details}
};

int rs_z80_out(int value)
{
  switch(state) {
  case RS_STATE_READY:
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
