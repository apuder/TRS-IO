
#pragma once

#include "trs-fs.h"

class BitstreamSource {
public:
  virtual ~BitstreamSource() {}
  virtual bool open() = 0;
  virtual bool read(void* buf, int n, int* br) = 0;
  virtual bool close() = 0;
};

class BitstreamSourceFile : public BitstreamSource {
private:
  const char* fn;
  FIL f;

public:
  BitstreamSourceFile(const char* fn) {
    this->fn = fn;
  }

  bool open() {
    if (trs_fs == NULL || trs_fs->get_err_msg() != NULL) {
      return false;
    }
    if (trs_fs->f_open(&f, fn, FA_READ) != FR_OK) {
      return false;
    }
    return true;
  }

  bool read(void* buf, int n, int* br) {
    UINT _br;

    if (trs_fs->f_read(&f, buf, n, &_br) != FR_OK) {
      return false;
    }
    *br = (int) _br;
    return true;
  }

  bool close() {
    if (trs_fs->f_close(&f) != FR_OK) {
      return false;
    }
    return true;
  }
};

class BitstreamSourceFlash : public BitstreamSource {
private:
  const char* fn;
  FILE* f;

public:
  BitstreamSourceFlash(const char* fn) {
    this->fn = fn;
  }

  bool open() {
    char* path;
    asprintf(&path, "/fpga/%s", fn);
    f = fopen(path, "rb");
    free(path);
    if (f == NULL) {
      return false;
    }
    return true;
  }

  bool read(void* buf, int n, int* br) {
    size_t _br;

    if (feof(f)) {
      return false;
    }
    _br = fread(buf, 1, n, f);
    *br = (int) _br;
    return true;
  }

  bool close() {
    fclose(f);
    return true;
  }
};

