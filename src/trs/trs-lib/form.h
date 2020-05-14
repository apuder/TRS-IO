
#ifndef __FORM_H__
#define __FORM_H__

#include "defs.h"
#include "window.h"
#include "key.h"
#include "panic.h"

#define FORM_OK 0
#define FORM_ABORT 1

#define FORM_TYPE_END 0
#define FORM_TYPE_HEADER 1
#define FORM_TYPE_INPUT 2
#define FORM_TYPE_CHECKBOX 3
#define FORM_TYPE_SELECT 4

#define FORM_CURSOR "\260"

typedef struct {
  uint8_t width;
  uint8_t len;
  char* buf;
} form_input_t;

typedef struct {
  bool* checked;
} form_checkbox_t;

typedef struct {
  int8_t first; // Used internally. Not a parameter
  uint8_t* selected;
  const char* (*items)[];
} form_select_t;
  
typedef struct {
  uint8_t type;
  const char* label;
  union {
    form_input_t input;
    form_checkbox_t checkbox;
    form_select_t select;
  } u;
} form_item_t;

uint8_t form(const char* title, form_item_t* form, bool show_from_left);

#endif
