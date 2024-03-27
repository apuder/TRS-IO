
#ifndef __KEYBOARDLAYOUT_H__
#define __KEYBOARDLAYOUT_H__

#include <stdint.h>
/****************************************************************
 * KeyboardLayout
 ****************************************************************/

#define DEFAULT_US_LAYOUT 3
//void init_KeyboardLayout();
//uint8_t getKeyboardLayout();
//void setKeyboardLayout(uint8_t layout);
uint8_t updateKbdLayout();
uint8_t updateKbdLayoutu8(uint8_t curLayoutIdx);
void configure_Keyboard_Layout();

#endif

