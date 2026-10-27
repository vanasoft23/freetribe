#ifndef LUT_H
#define LUT_H
#include <stdint.h>

extern s32 g_cutoff_lut[128];
extern s32 g_resonance_lut[256];

void lut_init();

#endif