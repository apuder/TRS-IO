
#include <trs-io.h>
#include <dos.h>
#include <inout.h>
#include <key.h>
#include <stdio.h>
#include <stdarg.h>
#include <xfer-params.h>


static const char* commands[] = {
  "DIR",
  "OPEN",
  "INIT",
  "READ",
  "WRITE",
  "CLOSE",
  "REMOVE",
  "RENAME"
};


static xfer_in_t in;
static xfer_out_t out;

static fcb_t fcb;
static uint16_t ern;

static bool quiet = false;

static void log(const char* fmt, ...)
{
  va_list argp;

  if (quiet) {
    return;
  }

  va_start(argp, fmt);
  vprintf(fmt, argp);
  va_end(argp);
}

static void usage()
{
  printf("Usage: xfer [-q]\n");
  printf("    -q: Quiet mode\n");
}

static void start_xfer()
{
  log("Starting XFER server\n");
  out31(XFER_MODULE_ID);
  out31(XFER_START);
  wait_for_esp();
}

static void stop_xfer()
{
  log("Stopping XFER server\n");
  out31(XFER_MODULE_ID);
  out31(XFER_STOP);
  wait_for_esp();
}

static uint16_t process_cmd_dir(cmd_in_dir_t* in, cmd_out_dir_t* out)
{
  dos_err_t err = dos_ramdir(0, in->drive, &out->entries);
  out->err = err;
  if (err != 0) {
    return 1;
  }
  uint16_t n = 0;
  while(out->entries[n].fname[0] != '+') {
    n++;
  }
  out->n = n;
  uint16_t size = n * sizeof(dir_t);
  return 1 + sizeof(n) + size;
}

static uint16_t process_cmd_open(cmd_in_open_t* in, cmd_out_open_t* out)
{
  dos_err_t err = dos_fspec(&in->fn[0], &fcb);
  if (err != NO_ERR) {
    out->err = err;
    return 1;
  }
  err = dos_open(&fcb, &((cmd_out_read_t*) out)->sector, 0);
  if (err != NO_ERR && err != ERR_LRL) {
    out->err = err;
    return 1;
  }

  ern = dos_getern(&fcb);
  out->err = NO_ERR;
  return 1;
}

static uint16_t process_cmd_init(cmd_in_init_t* in, cmd_out_init_t* out)
{
  dos_err_t err = dos_fspec(&in->fn[0], &fcb);
  if (err != NO_ERR) {
    out->err = err;
    return 1;
  }
  err = dos_init(&fcb, &((cmd_in_write_t*) in)->sector, 0);
  if (err != NO_ERR && err != ERR_LRL) {
    out->err = err;
    return 1;
  }

  ern = dos_getern(&fcb);
  out->err= NO_ERR;
  return 1;
}

static uint16_t process_cmd_read(cmd_in_read_t* in, cmd_out_read_t* out)
{
  (void) in;
  out->count = 0;
  out->err = NO_ERR;
  if (ern != 0) {
    ern--;
    uint8_t err = dos_read(&fcb, NULL);
    if (err != NO_ERR) {
      out->err = err;
      return 1;
    }
    out->count = (ern == 0) ? fcb.fname[8] : 256;
    if (out->count == 0) {
      out->count = 256;
    }
    return sizeof(cmd_out_read_t);
  }
  return 1 + sizeof(uint16_t);
}

static uint16_t process_cmd_write(cmd_in_write_t* in, cmd_out_write_t* out)
{
  dos_err_t err = dos_write(&fcb, NULL);
  if (err != NO_ERR) {
    out->err = err;
    return 1;
  }
  // Set EOF
  fcb.fname[8] = (in->count) & 0xff;
  dos_setern(&fcb);
  out->err = NO_ERR;
  return 1;
}

static uint16_t process_cmd_close(cmd_in_close_t* in, cmd_out_close_t* out)
{
  (void) in;
  out->err = dos_close(&fcb);
  return 1;
}

static uint16_t process_cmd_remove(cmd_in_remove_t* in, cmd_out_remove_t* out)
{
  dos_err_t err = dos_fspec(&in->fn[0], &fcb);
  if (err != NO_ERR) {
    out->err = err;
    return 1;
  }
  err = dos_open(&fcb, NULL, 0);
  if (err != NO_ERR && err != ERR_LRL) {
    out->err = err;
    return 1;
  }
  out->err = dos_remove(&fcb);
  return 1;
}

static uint16_t process_cmd_rename(cmd_in_rename_t* in, cmd_out_rename_t* out)
{
  out->err = dos_rename(&in->from[0], &in->to[0]);
  return 1;
}

static void process_cmd()
{
  uint16_t len = 0;

  log(commands[in.cmd]);
  if (in.cmd == XFER_CMD_OPEN) {
    log(": %.15s", in.params.open.fn);
  }
  if (in.cmd == XFER_CMD_INIT) {
    log(": %.15s", in.params.init.fn);
  }
  if (in.cmd == XFER_CMD_RENAME) {
    log(": %.15s -> %.15s", in.params.rename.from, in.params.rename.to);
  }
  log("\n");

  out31(XFER_MODULE_ID);
  out31(XFER_SEND_RESULT);

  switch(in.cmd) {
    case XFER_CMD_DIR:
      len = process_cmd_dir(&in.params.dir, &out.dir);
      break;
    case XFER_CMD_OPEN:
      len = process_cmd_open(&in.params.open, &out.open);
      break;
    case XFER_CMD_INIT:
      len = process_cmd_init(&in.params.init, &out.init);
      break;
    case XFER_CMD_READ:
      len = process_cmd_read(&in.params.read, &out.read);
      break;
    case XFER_CMD_WRITE:
      len = process_cmd_write(&in.params.write, &out.write);
      break;
    case XFER_CMD_CLOSE:
      len = process_cmd_close(&in.params.close, &out.close);
      break;
    case XFER_CMD_REMOVE:
      len = process_cmd_remove(&in.params.remove, &out.remove);
      break;
    case XFER_CMD_RENAME:
      len = process_cmd_rename(&in.params.rename, &out.rename);
      break;
  }
  // Send result to ESP
  out31(len & 0xff);
  out31(len >> 8);
  uint8_t* raw = (uint8_t*) &out;
  for (int i = 0; i < len; i++) {
    out31(*raw++);
  }
  wait_for_esp();
}

static void run_server()
{
  while(true) {
    out31(XFER_MODULE_ID);
    out31(XFER_NEXT_CMD);
    while (!is_esp_done()) {
      if (scan_key() == KEY_BREAK) {
        log("User abort\n");
        return;
      }
    }
    uint8_t bl = in31();
    uint8_t bh = in31();
    uint16_t len = bl | (bh << 8);
    if (len > sizeof(in)) {
      log("ERROR: command is too long (%d bytes)\n", len);
      continue;
    }
    uint8_t* raw = (uint8_t*) &in;
    for (int i = 0; i < len; i++) {
      *raw++ = in31();
    }
    process_cmd();
  }
}

int main(const char* args)
{
  if (strcmp(args, "-q") == 0) {
    quiet = true;
  } else if (args[0] != '\0') {
    usage();
    return -1;
  }

  if (trs_io_status() == TRS_IO_STATUS_NO_TRS_IO) {
    log("No TRS-IO card found!\n");
    return -1;
  }

  start_xfer();
  run_server();
  stop_xfer();
  return 0;
}