
#include <trs-io.h>
#include "serial.h"

#define TRS_FS_MODULE_ID 4

TRS_FS* trs_fs;

void TRS_FS::f_log(const char* format, ...) {
  char buf[80];
  va_list args;
  va_start(args, format);
  vsprintf(buf,format, args);
  va_end(args);
  f_log_impl(buf);
}

class TrsFileSystemModule : public TrsIO {
public:
  TrsFileSystemModule(int id) : TrsIO(id) {
    trs_fs = new TRS_FS_SERIAL();
  }
};

static TrsFileSystemModule theTrsFileSystemModule(TRS_FS_MODULE_ID);
