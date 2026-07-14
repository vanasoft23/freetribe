#ifndef EFFECT_H
#define EFFECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dsp_common.h"

#define NUM_EFFECTS 16

typedef enum {
    EFFECT_ID_WAVESHAPER,
    EFFECT_ID_OVERDRIVE,
    EFFECT_ID_FUZZ,
    EFFECT_ID_GLITCH,
    EFFECT_ID_FLANGER,
    EFFECT_ID___UNUSED__,
    EFFECT_ID_CHORUS,
    EFFECT_ID_PHASER,
    EFFECT_ID_DELAY_1_4,
    EFFECT_ID_DELAY_3_16,
    EFFECT_ID_DELAY_1_8,
    EFFECT_ID_DELAY_1_16,
    EFFECT_ID_DENSEVERB,
    EFFECT_ID_DISSOVERB,
    EFFECT_ID_LIMITER,
    EFFECT_ID_OTT,
} e_effect_id;

typedef struct t_effect t_effect;

typedef t_effect *(*fn_effect_init)();
typedef fract32   (*fn_effect_process)(t_effect *, fract32 input);
typedef void      (*fn_effect_set_param)(t_effect *, int param_id, fract32 value);
// typedef void   (*fn_effect_set_param_smooth)(t_effect *, int param_id, fract32 value);

struct t_effect {
    int32_t              effect_id;
    fn_effect_init       init;
    fn_effect_process    process;
    fn_effect_set_param  set_param;
    fract32              params[2];
};

extern const char *g_effect_name_str[NUM_EFFECTS];
extern const char *g_effect_param_str[NUM_EFFECTS][2];

#ifdef __cplusplus
}
#endif
#endif