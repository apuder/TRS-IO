
#include "retrostore.h"

void wait_for_esp()
{
  while (in(0xe0) & 8) ;
}

uint8_t scan()
{
  uint16_t i = 0;
  uint8_t status = RS_STATUS_NO_RETROSTORE_CARD;

  out(RS_PORT, RS_SEND_STATUS);

  while (++i != 0 && status == RS_STATUS_NO_RETROSTORE_CARD) {
    status = in(RS_PORT);
  }
  return status;
}

void get_version(uint8_t* revision, uint16_t* version)
{
  uint8_t version_minor;
  uint8_t version_major;

  out(RS_PORT, RS_SEND_VERSION);
  wait_for_esp();
  *revision = in(RS_PORT);
  version_major = in(RS_PORT);
  version_minor = in(RS_PORT);
  *version = (version_major << 8) | version_minor;
}
