
#ifndef __FREHD_H__
#define __FREHD_H__

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void frehd_check_action();
void init_frehd();
uint8_t frehd_in(uint8_t p);
void frehd_out(uint8_t p, uint8_t v);

#ifdef __cplusplus
}
#endif

#endif
