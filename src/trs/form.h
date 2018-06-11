
#ifndef __FORM_H__
#define __FORM_H__

#include "defs.h"
#include "window.h"
#include "key.h"
#include "panic.h"

#define FORM_OK 0
#define FORM_ABORT 1

#define FORM_TYPE_END 0
#define FORM_TYPE_TEXT 1
#define FORM_TYPE_INPUT 2

#define FORM_CURSOR "\260"

typedef struct {
  uint8_t len;
  const char* buf;
} form_input_t;

typedef struct {
  uint8_t type;
  int8_t x, y;
  union {
    const char* text;
    form_input_t input;
  } u;
} form_t;

uint8_t form(window_t* wnd, const char* title, form_t* form,
             bool show_from_left);

#endif
