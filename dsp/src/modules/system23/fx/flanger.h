#ifndef FLANGER_H
#define FLANGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dsp_common.h"

typedef struct t_flanger t_flanger;

t_flanger *flanger_init();
void       flanger_free(t_flanger *fl);
fract32    flanger_process(t_flanger *fl, fract32 input_sample);
void       flanger_set_param(t_flanger *fl, int param_index, fract32 value);
// void    flanger_set_param_smooth(t_flanger *fl, int param_index, fract32 value);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
