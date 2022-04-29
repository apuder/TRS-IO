
#include "retrostore.h"
#include "load.h"
#include "inout.h"
#include "esp.h"
#include "trs-lib.h"

typedef void (*pc_t)();

#define FILE_NAME_LEN 30

static char file_name[FILE_NAME_LEN + 1] = "";

static form_item_t form_load[] = {
  { FORM_TYPE_INPUT, "Filename", .u.input.len = FILE_NAME_LEN,
    .u.input.buf = file_name, .u.input.width = 0},
  { FORM_TYPE_END }
};


static void load()
{
  int i = 0;
  uint8_t b;
  uint16_t len;
  pc_t pc;
  uint8_t* video = (uint8_t*) 0x3c00;
  
  out(TRS_IO_PORT, TRS_IO_CORE_MODULE_ID);
  out(TRS_IO_PORT, TRS_IO_LOAD_XRAY_STATE);
  do {
    out(TRS_IO_PORT, file_name[i]);
  } while(file_name[i++] != '\0');

  wait_for_esp();

  b = in(TRS_IO_PORT);
  if (b != 0) goto err;
  pc = (pc_t) (in(TRS_IO_PORT) | (in(TRS_IO_PORT) << 8));
  len = in(TRS_IO_PORT) | (in(TRS_IO_PORT) << 8);
  if (len != 1024) goto err;

  for(i = 0; i < len; i++) {
    *video++ = in(TRS_IO_PORT);
  }

  (*pc)();
  
get_key();
  return;

err:
  wnd_popup("Error loading XRAY state");
get_key();
  
}

void load_xray_state()
{
  if (form("Load XRAY State", form_load, false) == FORM_ABORT) {
    return;
  }
  load();
}