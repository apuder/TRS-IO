
#include "fileio.h"
#include <string.h>

#define true 1
#define false 0

typedef int bool;

static void fail(int line) {
  char buf[20];
  sprintf(buf, "Err: %d", line);
  f_log(buf);
  while(1);
}

#define assert(c) if (!(c)) { fail(__LINE__); }

const char* FN1 = "ABC.TXT";
const char* FN2 = "XYZ.TXT";

const char* STR1 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char* STR2 = "abcdefghijk";

#define SIZE_BUF 1024
char buf[SIZE_BUF];


bool fileExists(const char* path) {
    FIL f;
    FRESULT r = f_open(&f, path, FA_READ);
    if (r == FR_OK) {
        f_close(&f);
        return true;
    }
    return false;
}

void checkFileContent(const char* fn, const char* s) {
    assert(fileExists(fn));
    FIL f;
    assert(f_open(&f, fn, FA_READ) == FR_OK);
    UINT br;
    assert(f_read(&f, buf, SIZE_BUF, &br) == FR_OK);
    assert(br == strlen(s));
    assert(strncmp(s, buf, strlen(s)) == 0);
    assert(f_close(&f) == FR_OK);
}

void test_1() {
    f_log("test_1");
    assert(!fileExists(FN1));
    FIL f;
    assert(f_open(&f, FN1, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
    UINT bw;
    assert(f_write(&f, STR1, strlen(STR1), &bw) == FR_OK);
    assert(bw == strlen(STR1));
    assert(f_close(&f) == FR_OK);
    checkFileContent(FN1, STR1);

    assert(f_open(&f, FN1, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
    assert(f_write(&f, STR2, strlen(STR2), &bw) == FR_OK);
    assert(bw == strlen(STR2));
    assert(f_close(&f) == FR_OK);
    checkFileContent(FN1, STR2);

    assert(f_unlink(FN1) == FR_OK);
    assert(!fileExists(FN1));
}

void test_2() {
    f_log("test_2");
    assert(!fileExists(FN1));
    FIL f;
    assert(f_open(&f, FN1, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
    UINT bw;
    assert(f_write(&f, STR1, strlen(STR1), &bw) == FR_OK);
    assert(bw == strlen(STR1));
    assert(f_close(&f) == FR_OK);

    assert(f_open(&f, FN1, FA_READ) == FR_OK);
    UINT br;
    int left = strlen(STR1);
    UINT p = 0;
    while (left > 0) {
        assert(f_read(&f, buf, 5, &br) == FR_OK);
        if (left > 5) {
            assert(br == 5);
        } else {
            assert(br == left);
        }
        assert(strncmp(STR1 + p, buf, br) == 0);
        p += 5;
        left -= 5;
    }
    assert(f_close(&f) == FR_OK);
    assert(f_unlink(FN1) == FR_OK);
    assert(!fileExists(FN1));
}

void test_3() {
    f_log("test_3");
    assert(!fileExists(FN1));
    FIL f;
    assert(f_open(&f, FN1, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) == FR_OK);
    UINT bw;
    assert(f_write(&f, STR1, strlen(STR1), &bw) == FR_OK);
    assert(bw == strlen(STR1));
    assert(f_lseek(&f, 5) == FR_OK);
    char buf[5];
    UINT br;
    assert(f_read(&f, buf, 5, &br) == FR_OK);
    assert(br == 5);
    assert(strncmp(STR1 + 5, buf, 5) == 0);
    assert(f_close(&f) == FR_OK);
    assert(f_unlink(FN1) == FR_OK);
}

void test_4() {
    DIR_ dp;
    assert(f_opendir(&dp, ".") == FR_OK);
    while (true) {
        FILINFO fi;
        assert(f_readdir(&dp, &fi) == FR_OK);
        if (fi.fname[0] == '\0') {
            break;
        }
        f_log("%s (%d/%d)", fi.fname, fi.fsize, fi.fattrib);
    }
}

int test_file_io() {
    test_1();
    test_2();
    test_3();
    test_4();
    f_log("All tests succeeded!");
    return 0;
}
