
#include <assert.h>
#include "frehd.h"
#include "action.h"
#include "trs_hard.h"
#include "trs_extra.h"
#include "loader_xtrs.h"


typedef uint8_t (*IO_READ_CB)();
typedef void (*IO_WRITE_CB)(uint8_t);

static uint8_t trs_read_wp() {
  return state_wp;
}

static uint8_t trs_read_control() {
  return state_control;
}

static uint8_t trs_read_data2() {
  uint8_t b = *((uint8_t*) (extra_buffer + state_bytesdone2));
  state_bytesdone2++;
  return b;
}

static uint8_t trs_read_size2() {
  return state_size2;
}

static uint8_t trs_read_rom() {
  if (!trs_fs_mounted()) {
    return 0xff;
  }
  uint8_t b = (state_romdone == 2) ? state_rom : loader[state_romdone];
  state_romdone++;
  return b;
}

static uint8_t trs_read_error2() {
  return state_error2;
}

static uint8_t trs_read_uart_status() {
  return 0xff;
}

static uint8_t trs_read_uart() {
  return 0xff;
}

static uint8_t trs_read_data() {
  uint8_t b = *((uint8_t*) (sector_buffer + state_bytesdone));
  state_bytesdone++;
  if (state_bytesdone == state_secsize16) {
    state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
  }
  return b;
}

static uint8_t trs_read_error() {
  return state_error;
}

static uint8_t trs_read_seccnt() {
  return state_seccnt;
}

static uint8_t trs_read_secnum() {
  return state_secnum;
}

static uint8_t trs_read_cyllo() {
  return state_cyl & 0xff;
}

static uint8_t trs_read_cylhi() {
  return (state_cyl >> 8) & 0xff;
}

static uint8_t trs_read_sdh() {
  return (state_drive << 3) | state_head;
}

static uint8_t trs_read_status() {
  return state_status;
}


          
static IO_READ_CB io_read_cb[] = {
  trs_read_wp,       // C0
  trs_read_control,  // C1
  trs_read_data2,    // C2
  trs_read_size2,    // C3
  trs_read_rom,      // C4
  trs_read_error2,   // C5
  trs_read_uart_status, // C6 
  trs_read_uart,     // C7
  trs_read_data,     // C8
  trs_read_error,    // C9
  trs_read_seccnt,   // CA
  trs_read_secnum,   // CB
  trs_read_cyllo,    // CC
  trs_read_cylhi,    // CD
  trs_read_sdh,      // CE
  trs_read_status    // CF
};



static void int_ret_rd2(uint8_t v) {
  // NOP
}

static void trs_write_control(uint8_t v) {
  state_control = v;
  if (v & TRS_HARD_DEVICE_ENABLE) {
    return;
  }
  if (state_present == 0) {
    state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR;
    state_error = TRS_HARD_NFERR;
  } else {
    state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
  }
}

static void trs_write_data2(uint8_t v) {
  *((uint8_t*) (extra_buffer + state_bytesdone2)) = v;
  state_bytesdone2++;
  if (state_bytesdone2 == state_size2) {
    action_type = state_command2 | ACTION_EXTRA2;
    state_status = TRS_HARD_BUSY | TRS_HARD_CIP;
    action_flags |= ACTION_TRS;
  }
}

static void trs_write_size2(uint8_t v) {
  state_size2 = (v == 0) ? 0x100 : v;
}

static void trs_write_command2(uint8_t v) {
  state_command2 = v;
  state_bytesdone2 = 0;
  action_type = state_command2 | ACTION_EXTRA;
  state_status = TRS_HARD_BUSY | TRS_HARD_CIP;
  action_flags |= ACTION_TRS;
}

static void trs_write_rom(uint8_t v) {
  state_rom = v;
  state_romdone = 0;
}

static void trs_write_uart_ctrl(uint8_t v) {
  assert(0);
}

static void trs_write_uart(uint8_t v) {
  assert(0);
}

static void trs_write_data(uint8_t v) {
  if ((state_command ^ (TRS_HARD_WRITE >> 4)) != 0) {
    return;
  }
  *((uint8_t*) (sector_buffer + state_bytesdone)) = v;
  state_bytesdone++;
  if (state_bytesdone != state_secsize16) {
    return;
  }
  state_status = TRS_HARD_BUSY | TRS_HARD_CIP;
  action_type = ACTION_HARD_WRITE;
  action_flags |= ACTION_TRS;
}

static void trs_write_seccnt(uint8_t v) {
  state_seccnt = v;
}

static void trs_write_secnum(uint8_t v) {
  state_secnum = v;
}

static void trs_write_cyllo(uint8_t v) {
  state_cyl = (state_cyl & 0xff00) | (v & 0x00ff);
}

static void trs_write_cylhi(uint8_t v) {
  state_cyl = (state_cyl & 0x00ff) | ((v << 8) & 0xff00);
}

static void trs_write_sdh(uint8_t v) {
  state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
  state_secsize16 = 0;
  state_secsize = (v >> 5) & 3;
  switch (state_secsize) {
  case 0:
    state_secsize16 = 0x100;
    break;
  case 1:
    state_secsize16 = 0x200;
    break;
  case 2:
    state_secsize16 = 0x400;
    break;
  case 3:
    state_secsize16 = 0x80;
    break;
  }
  state_drive = (v >> 3) & 3;
  state_head = v & 7;
}

static void trs_write_cmd_restore(uint8_t v) {
  state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
  state_cyl = 0;
}

static void trs_write_cmd_read(uint8_t v) {
  state_status = TRS_HARD_BUSY | TRS_HARD_CIP;
  action_type = ACTION_HARD_READ;
  action_flags |= ACTION_TRS;
}

static void trs_write_cmd_write(uint8_t v) {
  if ((v & TRS_HARD_MULTI) == 0) {
    state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_DRQ;
  } else {
    state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE | TRS_HARD_ERR;
    state_error = TRS_HARD_ABRTERR;
  }
}

static void trs_write_cmd_verify_seek(uint8_t v) {
  state_status = TRS_HARD_BUSY | TRS_HARD_CIP;
  action_type = ACTION_HARD_SEEK;
  action_flags |= ACTION_TRS;
}

static void trs_write_cmd_init_format_nop(uint8_t v) {
  state_status = TRS_HARD_READY | TRS_HARD_SEEKDONE;
}

static void trs_write_cmd_test(uint8_t v) {
  state_status = TRS_HARD_READY;
  state_error = 0;
}

static IO_WRITE_CB io_write_cb[] = {
  int_ret_rd2,       // C0
  trs_write_control, // C1
  trs_write_data2,   // C2
  trs_write_size2,   // C3
  trs_write_command2, // C4
  trs_write_rom,     // C5
  trs_write_uart_ctrl, // C6
  trs_write_uart,    // C7
  trs_write_data,    // C8
  int_ret_rd2,       // C9
  trs_write_seccnt,  // CA
  trs_write_secnum,  // CB
  trs_write_cyllo,   // CC
  trs_write_cylhi,   // CD
  trs_write_sdh,     // CE
  int_ret_rd2,       // CF
  trs_write_cmd_restore, // CF (restore)
  trs_write_cmd_read,    // CF (read)
  trs_write_cmd_write,   // CF (write)
  trs_write_cmd_verify_seek,  // CF (verify)
  trs_write_cmd_init_format_nop,  // CF (format)
  trs_write_cmd_init_format_nop,    // CF (init)
  trs_write_cmd_verify_seek,    // CF (seek)
  trs_write_cmd_init_format_nop,     // CF (no operation)
  trs_write_cmd_test,    // CF (test - WD1002)
  trs_write_cmd_init_format_nop,     // CF (no operation)
  trs_write_cmd_init_format_nop,     // CF (no operation)
  trs_write_cmd_init_format_nop,     // CF (no operation)
  trs_write_cmd_init_format_nop,     // CF (no operation)
  trs_write_cmd_init_format_nop,     // CF (no operation)
  trs_write_cmd_init_format_nop,     // CF (no operation)
  trs_write_cmd_init_format_nop      // CF (no operation)
};



uint8_t frehd_in(uint8_t p) {
  return io_read_cb[p & 0x0f]();
}

void frehd_out(uint8_t p, uint8_t v) {
  p = p & 0x0f;
  if (p == 0x0f) {
    v = (v >> 4) & 0x0f;
    state_command = v;
    state_bytesdone = 0;
    p = v + 0x0f;
  }
  io_write_cb[p](v);
}
