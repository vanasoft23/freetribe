#ifndef OTT_H
#define OTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dsp_common.h"

typedef struct t_ott t_ott;

t_ott     *ott_init();
void       ott_free(t_ott *ott);
fract32    ott_process(t_ott *ott, fract32 input_sample);
void       ott_set_param(t_ott *ott, int param_index, fract32 value);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
