#ifndef DELAYLINE_H
#define DELAYLINE_H

#include "types.h"

typedef struct {
    fract32 *buffer;
    uint32_t write_offset;
    uint32_t read_offset;
    uint32_t delay_length;
    fract32 feedback;
} t_delayline;

void    delayline_init(t_delayline *dl);
void    delayline_free(t_delayline *dl);
fract32 delayline_process(t_delayline *dl, fract32 input_sample);

#endif