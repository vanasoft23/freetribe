
/*----- Includes -----------------------------------------------------*/

#include "aleph_lpf_one_pole.h"
#include "dsp_common.h"
#include "delayline.h"
#include "effect.h"
#include "ott.h"

/*----- Macros -------------------------------------------------------*/

#define PARAM_ID_THRESHOLD 0
#define PARAM_ID_TIME      1

#define PARAM_SLEW      0x00010000
#define ATTACK_COEFF    0x053947A6 // 0.5 ms → α = 0.040810543 → Q1.31
#define RELEASE_COEFF   0x000DA686 //  50 ms → α = 0.000416580 → Q1.31

/*----- Typedefs -----------------------------------------------------*/

struct t_ott {
	t_effect         base;
	Aleph_LPFOnePole param_lpf[2];
	fract32 env;
	fract32 gain;
};


/*----- Extern variable declarations ---------------------------------*/

extern t_Aleph g_aleph;

/*----- Static function declarations ---------------------------------*/

inline fract32 q31_signed_to_unsigned(fract32 x) {
	// (x + 1.0) * 0.5
	// 1.0 = 0x7FFFFFFF
	fract32 tmp = add_fr1x32(x, 0x7FFFFFFF);  // add 1.0
	return shr_fr1x32(tmp, 1);                // divide by 2
}

/*----- Extern function implementations ------------------------------*/

t_ott *ott_init() {

	t_ott *ott = malloc(sizeof(t_ott));

	ott->base.effect_id        = EFFECT_ID_OTT;
	ott->base.init             = (fn_effect_init)&ott_init;
	ott->base.process          = (fn_effect_process)&ott_process;
	ott->base.set_param        = (fn_effect_set_param)&ott_set_param;

	Aleph_LPFOnePole_init(&ott->param_lpf[0], &g_aleph);
	Aleph_LPFOnePole_set_coeff(&ott->param_lpf[0], PARAM_SLEW);
	Aleph_LPFOnePole_init(&ott->param_lpf[1], &g_aleph);
	Aleph_LPFOnePole_set_coeff(&ott->param_lpf[1], PARAM_SLEW);
	
	return ott;
}

void ott_free(t_ott *ott) {

}

fract32 ott_process(t_ott *ott, fract32 input_sample) {

	// fract32 threshold = Aleph_LPFOnePole_next(&ott->param_lpf[PARAM_ID_THRESHOLD]);
	// threshold = q31_signed_to_unsigned(threshold);
	const fract32 threshold = 0x40000000;

	// Envelope follower
	fract32 abs_input_sample = abs_fr1x32(input_sample);
	fract32 coeff = (abs_input_sample > ott->env) ? ATTACK_COEFF : RELEASE_COEFF;
	ott->env = add_fr1x32(
		mult_fr1x32x32(sub_fr1x32(abs_input_sample, ott->env), coeff),
		ott->env);

	// Compute target gain
	fract32 gain_target;
	fract32 diff = sub_fr1x32(ott->env, threshold);
	if (diff < 0)
		gain_target = FR32_MAX;
	else {
		// approximate gain drop
		fract32 k = 0x40000000; // 0.5 slope, tune for your needs
		fract32 drop = mult_fr1x32x32(diff, k);
		gain_target = sub_fr1x32(FR32_MAX, drop);
	}

	// // gain_target      = 1-(overshoot^2)
	// fract32 overshoot   = max_fr1x32(0, sub_fr1x32(ott->env, threshold));
	// fract32 reduction   = mult_fr1x32x32(overshoot, overshoot);
	// fract32 gain_target = sub_fr1x32(FR32_MAX, reduction);

	// Smooth gain (optional)
	ott->gain = add_fr1x32(ott->gain, 
		mult_fr1x32x32(sub_fr1x32(gain_target, ott->gain), RELEASE_COEFF));
	
	return mult_fr1x32x32(input_sample, ott->gain);
}

void ott_set_param(t_ott *ott, int param_index, fract32 value) {

	ott->base.params[param_index] = value;
	Aleph_LPFOnePole_set_target(&ott->param_lpf[param_index], value);

}


/*----- Static function implementations ------------------------------*/


/*----- End of file --------------------------------------------------*/
