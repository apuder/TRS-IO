
#include "retrostore.h"
#include "backend.h"
#include "boot.c"
#include "rsclient.c"

#include <string.h>


static int state = RS_STATE_READY;

static int cmd;
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

static void command_send_boot(uint16_t idx)
{
  send(boot_bin, boot_bin_len);
}

static void command_send_cmd(uint16_t idx)
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

static void command_send_app_title(uint16_t idx)
{
  char* title = get_app_title(idx);
  send((uint8_t*) title, strlen(title) + 1);
}

static void command_send_app_details(uint16_t idx)
{
  char* details = get_app_details(idx);
  send((uint8_t*) details, strlen(details) + 1);
}


typedef void (*proc_t)(uint16_t);

typedef struct {
  bool needs_idx;
  proc_t proc;
} command_t;

static command_t commands[] = {
  {false, command_send_boot},
  {true, command_send_cmd},
  {true, command_send_app_title},
  {true, command_send_app_details}
};

int rs_z80_out(int value)
{
  switch(state) {
  case RS_STATE_READY:
    cmd = value;
    if (!commands[cmd].needs_idx) {
      commands[cmd].proc(0);
    } else {
      state = RS_STATE_NEED_2B;
    }
    break;
  case RS_STATE_NEED_2B:
    b1 = value;
    state = RS_STATE_NEED_1B;
    break;
  case RS_STATE_NEED_1B:
    commands[cmd].proc(b1 | (b2 << 8));
    break;
  default:
    // Ignore
    break;
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
