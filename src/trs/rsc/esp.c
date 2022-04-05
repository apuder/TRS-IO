
#include "retrostore.h"

static inline bool is_m3()
{
  return *((uint8_t*) 0x125) == 'I';
}

void wait_for_esp()
{
  if (is_m3()) {
    while (in(0xe0) & 8) ;
  } else {
    while (*((uint8_t*) 0x37e0) & 0x20) ;
  }
}

uint8_t scan()
{
  uint16_t i = 0;
  uint8_t status = RS_STATUS_NO_RETROSTORE_CARD;

  out(TRS_IO_PORT, TRS_IO_CORE_MODULE_ID);
  out(TRS_IO_PORT, TRS_IO_SEND_STATUS);

  while (++i != 0 && status == RS_STATUS_NO_RETROSTORE_CARD) {
    status = in(TRS_IO_PORT);
  }
  return status;
}

void get_trs_io_version(uint8_t* revision, uint16_t* version)
{
  uint8_t version_minor;
  uint8_t version_major;

  out(TRS_IO_PORT, TRS_IO_CORE_MODULE_ID);
  out(TRS_IO_PORT, RS_SEND_VERSION);
  wait_for_esp();
  *revision = in(TRS_IO_PORT);
  version_major = in(TRS_IO_PORT);
  version_minor = in(TRS_IO_PORT);
  *version = (version_major << 8) | version_minor;
}

void get_retrostore_version(uint16_t* version)
{
  uint8_t version_minor;
  uint8_t version_major;

  out(TRS_IO_PORT, RETROSTORE_MODULE_ID);
  out(TRS_IO_PORT, RS_SEND_VERSION);
  wait_for_esp();
  version_major = in(TRS_IO_PORT);
  version_minor = in(TRS_IO_PORT);
  *version = (version_major << 8) | version_minor;
}

bool has_xray_support()
{
  out(TRS_IO_PORT, TRS_IO_CORE_MODULE_ID);
  out(TRS_IO_PORT, TRS_IO_LOAD_XRAY_STATE);
  // Send empty string as file name
  out(TRS_IO_PORT, 0);
  wait_for_esp();
  return in(TRS_IO_PORT) != 0xfe;
}

