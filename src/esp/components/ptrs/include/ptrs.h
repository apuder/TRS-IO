
#ifndef __PTRS_H__
#define __PTRS_H__

#define PTRS_CONFIG_DIP_1      (1 << 0)
#define PTRS_CONFIG_DIP_2      (1 << 1)
#define PTRS_CONFIG_DIP_3      (1 << 2)
#define PTRS_CONFIG_DIP_4      (1 << 3)
#define PTRS_CONFIG_HIRES      (1 << 4)
#define PTRS_CONFIG_WIDE       (1 << 5)
#define PTRS_CONFIG_80_COLS    (1 << 6)
#define PTRS_CONFIG_MODEL_MASK (PTRS_CONFIG_DIP_1 | PTRS_CONFIG_DIP_2 | PTRS_CONFIG_DIP_3)
#define PTRS_CONFIG_MODEL_1    (PTRS_CONFIG_DIP_1)
#define PTRS_CONFIG_MODEL_3    (PTRS_CONFIG_DIP_1 | PTRS_CONFIG_DIP_2)
#define PTRS_CONFIG_MODEL_4    (PTRS_CONFIG_DIP_3)
#define PTRS_CONFIG_MODEL_4P   (PTRS_CONFIG_DIP_1 | PTRS_CONFIG_DIP_3)

void configure_pocket_trs(bool is_80_cols);
bool configure_ptrs_settings();
void ptrs_status();
void init_trs_lib(bool is_80_cols);
void ptrs_load_rom();
void init_ptrs(int model);


#endif

