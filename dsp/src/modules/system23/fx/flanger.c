
/*----- Includes -----------------------------------------------------*/

#include "aleph_lpf_one_pole.h"
#include "dsp_common.h"
#include "delayline.h"
#include "effect.h"
#include "flanger.h"

/*----- Macros -------------------------------------------------------*/

// #define MAX_DELAY_MS            50
// #define MAX_DELAY_SAMPLES       (MAX_DELAY_MS*SAMPLERATE/1000)

#define MIN_DELAY_SAMPLES       14
#define MAX_DELAY_SAMPLES       (2<<16) // must be smaller than DELAY_BUFFER_SIZE
#define PARAM_SLEW              0x00010000

/*----- Typedefs -----------------------------------------------------*/

struct t_flanger {
    t_effect         base;
    t_delayline      delayline;
    Aleph_LPFOnePole param_lpf[2];
};

/*----- Extern variable declarations ---------------------------------*/

extern t_Aleph g_aleph;

/*----- Static function declarations ---------------------------------*/

static uint32_t _compute_delay_length(fract32 delay_param);
static fract32  _compute_feedback(fract32 intensity_param);
static fract32  _compute_mix(fract32 intensity_param);

/*----- Extern function implementations ------------------------------*/

t_flanger *flanger_init() {

    t_flanger *fl = malloc(sizeof(t_flanger));

    fl->base.effect_id        = EFFECT_ID_FLANGER;
    fl->base.init             = (fn_effect_init)&flanger_init;
    fl->base.process          = (fn_effect_process)&flanger_process;
    fl->base.set_param        = (fn_effect_set_param)&flanger_set_param;
    // fl->base.set_param_smooth = (fn_effect_set_param_smooth)&flanger_set_param_smooth;
    delayline_init(&fl->delayline);
    // delayline_set_delay_length(&fl->delayline, 10000);

    Aleph_LPFOnePole_init(&fl->param_lpf[0], &g_aleph);
    Aleph_LPFOnePole_set_coeff(&fl->param_lpf[0], PARAM_SLEW);
    Aleph_LPFOnePole_init(&fl->param_lpf[1], &g_aleph);
    Aleph_LPFOnePole_set_coeff(&fl->param_lpf[1], PARAM_SLEW);
    
    return fl;
}

void flanger_free(t_flanger *fl) {

    Aleph_LPFOnePole_free(&fl->param_lpf[0]);
    Aleph_LPFOnePole_free(&fl->param_lpf[1]);
    delayline_free(&fl->delayline);

}

fract32 flanger_process(t_flanger *fl, fract32 input_sample) {

    fract32 param0_smoothed    = Aleph_LPFOnePole_next(&fl->param_lpf[0]);
    fract32 param1_smoothed    = Aleph_LPFOnePole_next(&fl->param_lpf[1]);
    fl->delayline.delay_length = _compute_delay_length(param0_smoothed);
    fl->delayline.feedback     = _compute_feedback(param1_smoothed);
    fract32 mix                = _compute_mix(param1_smoothed);
    
    fract32 delayed_sample = delayline_process(&fl->delayline, input_sample);

    fract32 output_sample = add_fr1x32(
        mult_fr1x32x32(input_sample, sub_fr1x32(FR32_MAX, mix)),
        mult_fr1x32x32(delayed_sample, mix)
    );
    
    return output_sample;
}

void flanger_set_param(t_flanger *fl, int param_index, fract32 value) {

    fl->base.params[param_index] = value;
    Aleph_LPFOnePole_set_target(&fl->param_lpf[param_index], value);

}

// void flanger_set_param_smooth(t_flanger *fl, int param_index, fract32 value) {

//     fl->base.params[param_index] = value;
//     Aleph_LPFOnePole_set_target(&fl->param_lpf[param_index], value);

// }

/*----- Static function implementations ------------------------------*/

static uint32_t _compute_delay_length(fract32 delay_param) {

    // 1. Convert [-1.0, +1.0] → [0.0, 1.0] (Q1.31 → Q0.32)
    uint32_t u = (uint32_t)delay_param + 0x80000000u;

    // 2. Apply nonlinear shaping: (x^2)
    //    multiply Q0.32 * Q0.32 → Q0.32 result
    uint64_t temp = (uint64_t)u * (uint64_t)u;
    u = (uint32_t)(temp >> 32);

    // 3. Scale to [5, MAX_DELAY_SAMPLES]
    const uint32_t range = MAX_DELAY_SAMPLES - MIN_DELAY_SAMPLES;
    uint32_t delay = MIN_DELAY_SAMPLES + (uint32_t)(((uint64_t)u * range) >> 32);

    return delay;
}

static fract32 _compute_feedback(fract32 intensity_param) {

    // Feedback shouldn't be 100% in order to avoid Mexican dog
    return mult_fr1x32x32(intensity_param, 0x7951EB85);
}

/**
 * @brief  =base + (1 - base) * (abs(intensity_param) ^ p)
 */
static fract32 _compute_mix(fract32 intensity_param) {
    
    const fract32 base  = 0x33333333; // 0.4
    const fract32 scale = 0x4CCCCCCD; // 0.6 (remaining value)

    fract32 y;
    y = abs_fr1x32(intensity_param);
    y = mult_fr1x32x32(y, y); // p=2
    y = mult_fr1x32x32(y, y); // p=4
    y = add_fr1x32(base, mult_fr1x32x32(y, scale));

    return y;
}

/*----- End of file --------------------------------------------------*/
