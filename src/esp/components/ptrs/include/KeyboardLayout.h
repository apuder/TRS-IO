
#ifndef __KEYBOARDLAYOUT_H__
#define __KEYBOARDLAYOUT_H__

#include <stdint.h>
#include "fabgl.h"
/****************************************************************
 * KeyboardLayout
 ****************************************************************/

static const char* kbitems[] = {
  "DE",
  "IT",
  "UK",
  "US",
  "ES",
  "FR",
  "BE",
  "NO",
  "JP",
  NULL};



uint8_t updateKbdLayout();
uint8_t updateKbdLayoutu8(uint8_t curLayoutIdx);
#endif

