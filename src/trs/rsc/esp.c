
#include "retrostore.h"

void get_trs_io_version(uint8_t* revision, uint16_t* version)
{
  uint8_t version_minor;
  uint8_t version_major;

  out31(TRS_IO_CORE_MODULE_ID);
  out31(RS_SEND_VERSION);
  wait_for_esp();
  *revision = in31();
  version_major = in31();
  version_minor = in31();
  *version = (version_major << 8) | version_minor;
}

void get_retrostore_version(uint16_t* version)
{
  uint8_t version_minor;
  uint8_t version_major;

  out31(RETROSTORE_MODULE_ID);
  out31(RS_SEND_VERSION);
  wait_for_esp();
  version_major = in31();
  version_minor = in31();
  *version = (version_major << 8) | version_minor;
}

bool has_xray_support()
{
  out31(TRS_IO_CORE_MODULE_ID);
  out31(TRS_IO_LOAD_XRAY_STATE);
  // Send empty string as file name
  out31(0);
  wait_for_esp();
  return in31() != 0xfe;
}

