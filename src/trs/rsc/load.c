
#include "retrostore.h"
#include "load.h"
#include "inout.h"
#include "esp.h"
#include "trs-lib.h"

typedef void (*pc_t)();

#define TOKEN_LEN 3

static char token[TOKEN_LEN + 1] = "";

static form_item_t form_load_items[] = {
  FORM_ITEM_INPUT("Token", token, TOKEN_LEN, 0, NULL),
  FORM_ITEM_END
};

static form_t form_load = {
	.title = "Load XRAY State",
	.form_items = form_load_items
};

static void load()
{
  int i = 0;
  uint8_t b;
  uint16_t len;
  pc_t pc;
  uint8_t* video = (uint8_t*) 0x3c00;
  
  out31(TRS_IO_CORE_MODULE_ID);
  out31(TRS_IO_LOAD_XRAY_STATE);
  do {
    out31(token[i]);
  } while(token[i++] != '\0');

  wait_for_esp();

  b = in31();
  if (b != 0) goto err;
  pc = (pc_t) (in31() | (in31() << 8));
  len = in31() | (in31() << 8);
  if (len == 1024) {
    // We have a screenshot
    for(i = 0; i < len; i++) {
      *video++ = in31();
    }
  }

  (*pc)();
  
  // Never reached
  get_key();
  return;

err:
  wnd_popup("Error loading XRAY state");
  get_key();
}

void load_xray_state()
{
  if (form(&form_load, false) == FORM_ABORT) {
    return;
  }
  load();
}
