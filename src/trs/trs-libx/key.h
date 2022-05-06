
#ifndef __KEY_H__
#define __KEY_H__

typedef char (*get_key_t)();

#define KEY_ENTER 0x0d
#define KEY_BREAK 0x01
#define KEY_CLEAR 0x1f

#define KEY_UP 0x5b
#define KEY_DOWN 0x0a
#define KEY_RIGHT 0x09
#define KEY_LEFT 0x08

void set_keyboard_callback(get_key_t get_key);
char get_key();

#endif
