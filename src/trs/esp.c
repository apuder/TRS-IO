
#include "retrostore.h"

void wait_for_esp()
{
  while (in(0xe0) & 8) ;
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
