
/*----- Includes -----------------------------------------------------*/

#include "dsp_common.h"
#include "fix.h"
#include <string.h>

#include "delayline.h"

/*----- Macros -------------------------------------------------------*/

#define DELAY_BUFFER_SIZE (2<<17)

// #define FR32_ZERO_POINT_FIVE 0x80000000


// ///////////// BEGIN malloc stuff

// #define MEM_OFFSET (16*1024*1024)
// extern void sdram_start;

// static void *malloc(int n) {
//     return ((uint8_t*)&sdram_start+MEM_OFFSET);
// }

// static void *calloc(int n) {
//     void *ptr = malloc(n);
//     memset(ptr, 0, n);
//     return ptr;
// }

// static void free(void *ptr) { (void)ptr; }

// ///////////// END malloc stuff


/*----- Extern function implementations ------------------------------*/

void delayline_init(t_delayline *dl) {

    dl->buffer = (fract32*)malloc(DELAY_BUFFER_SIZE);
    memset(dl->buffer, 0, DELAY_BUFFER_SIZE);
    dl->write_offset = 0;

}


void delayline_free(t_delayline *dl) {

    free(dl->buffer);

}


fract32 delayline_process(t_delayline *dl, fract32 input_sample) {

    dl->read_offset = (dl->write_offset + DELAY_BUFFER_SIZE - dl->delay_length) % DELAY_BUFFER_SIZE;
    fract32 delayed_sample = dl->buffer[dl->read_offset];

    dl->buffer[dl->write_offset] = add_fr1x32(
        input_sample,
        mult_fr1x32x32(delayed_sample, dl->feedback)
    );

    dl->write_offset = (dl->write_offset + 1) % DELAY_BUFFER_SIZE;

    return delayed_sample;
}


