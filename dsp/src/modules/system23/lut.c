#include "dsp_common.h"
#include "lut.h"

// #define LUT_SIZE 512
// static const fract32 s_reciprocal_lut[LUT_SIZE] = { /* 1/(1 + i/LUT_SIZE) scaled Q1.31 */ };

void lut_init() {

}

// inline fract32 fast_div(fract32 num, fract32 denom) {
//     uint32_t idx = (uint32_t)(denom >> 23); // top 8 bits as index
//     fract32 recip = s_reciprocal_lut[min(idx, LUT_SIZE-1)];
//     return mult_fr1x32x32(num, recip);
// }
