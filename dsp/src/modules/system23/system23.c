
/*----- Includes -----------------------------------------------------*/

#include "dsp_common.h"

#include "module.h"

#include "utils.h"

#include "aleph.h"
#include "aleph_monovoice.h"

#include "dsp_playhead.h"
#include "pattern_cache.h"
#include "param_defs.h"

#include "flanger.h"
#include "ott.h"

/*----- Macros -------------------------------------------------------*/

#define BLOCK_SIZE 256

#define MEMPOOL_SIZE (0x1000)


/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	Aleph_MonoVoice voice[1];
	bool gate_on;
	uint32_t gate_ticks;
	fract32 note_freq;
	uint32_t ticks_til_next;
	fract32 next_note_freq;

	float cutoff;
	float resonance;
} t_module;

/*----- Static variable definitions ----------------------------------*/

__attribute__((section(".l1.data.a")))
	__attribute__((aligned(32))) static char g_mempool[MEMPOOL_SIZE];

// static float t_tb303pattern g_pattern_notes[255]; // full module transmit buffer

static t_module   g_module;
static t_flanger *g_flanger;
static t_ott     *g_ott;

/*----- Extern variable definitions ----------------------------------*/

t_Aleph g_aleph;

/*----- Static function prototypes -----------------------------------*/

// static uint32_t seed = 1; // Initialize with a non-zero value
// uint32_t rand_uint32(void) {
//     seed = (1664525 * seed + 1013904223);
//     return seed;
// }

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise module.
 */
void module_init(void) {

	ptn_cache_init();

	Aleph_init(&g_aleph, SAMPLERATE, g_mempool, MEMPOOL_SIZE, NULL);
	Aleph_MonoVoice_init(&g_module.voice[0], &g_aleph);
	Aleph_MonoVoice_set_amp(&g_module.voice[0], FR32_MAX);
	Aleph_MonoVoice_set_amp_slew(&g_module.voice[0], SLEW_1MS);
	Aleph_MonoVoice_set_shape(&g_module.voice[0], WAVEFORM_SHAPE_SAW);
	Aleph_MonoVoice_set_freq_offset(&g_module.voice[0], FIX16_ONE);
	Aleph_MonoVoice_set_cutoff(&g_module.voice[0], 0x326f6abb);
	Aleph_MonoVoice_set_res(&g_module.voice[0], FR32_MAX);

	// delayline_init(&g_delayline, 19200, 0x43000000, 0x33333333); // ((60000/150)/1000)*48000
	g_flanger = flanger_init(g_flanger);
	flanger_set_param(g_flanger, 0, 0x43000000);
	flanger_set_param(g_flanger, 1, 0x33333333);

	g_ott = ott_init(g_ott);
	ott_set_param(g_ott, 0, 0x4000000);
	ott_set_param(g_ott, 1, 0x4000000);

	dsp_playhead_set_pos(0);
	dsp_playhead_set_tempo(180);
	dsp_playhead_playstop(true);
	
}

/**
 * @brief   Process audio.
 *
 * @param[in]   in  Pointer to input buffer.
 * @param[out]  out Pointer to input buffer.
 */
void module_process(fract32 *in, fract32 *out) {

	t_pattern *ptn = ptn_cache_fetch_latest();
	
	// @TODO: implement block processing
	for (int frame = 0; frame < BLOCK_SIZE; frame++) {

		// for (int part_idx = 0; part_idx < 16; part_idx++) {
		//     t_part *part = &ptn->parts[part_idx];

		//     if (dsp_playhead_should_trigger()) {
		//         // @TODO: handle state transition
		//     }

		// }
		
		dsp_playhead_advance(BLOCK_SIZE);
	}

	dsp_playhead_report();

	// for (int i = 0; i < BLOCK_SIZE; i++) {

	//     if (g_module.gate_ticks > 0)
	//         g_module.gate_ticks--;
		
	//     if (g_module.gate_on) {
	//         // Wait til next note has to play when gate hold is over
	//         if (g_module.gate_ticks == 0) {
	//             g_module.gate_on = false;
	//             g_module.gate_ticks = g_module.ticks_til_next;
	//         }
	//     } else {
	//         // Start playing cached note if the DSP is earlier than the CPU can feed us notes
	//         if (g_module.gate_ticks == 0) {
	//             g_module.gate_on = true;
	//             g_module.note_freq = g_module.next_note_freq;
	//         }
	//     }

	//     Aleph_MonoVoice_set_amp(&g_module.voice[0], g_module.gate_on ? FR32_MAX : 0x00000000);
	//     Aleph_MonoVoice_set_freq(&g_module.voice[0], g_module.note_freq);
	//     fract32 sample = Aleph_MonoVoice_next(&g_module.voice[0]);

	//     // Additional processing
	//     sample = shr_fr1x32(sample, 1); // temporarily lower amplitude to prevent clipping
	//     sample = flanger_process(g_flanger, sample);
	//     sample = ott_process(g_ott, sample);

	//     out[0] = sample;
	//     out[1] = sample;
	
	// }

}

/**
 * @brief   Set parameter.
 *
 * @param[in]   param_index Index of parameter to set.
 * @param[in]   value      Value of parameter.
 */
void module_set_param(uint16_t param_index, int32_t value) {

	switch (param_index) {

		// case PARAM_NOTE_FREQ:
		//     g_module.note_freq = value;
		//     break;
		// case PARAM_GATE_TICKS:
		//     g_module.gate_ticks = value;
		//     g_module.gate_on = true; // reset to make sure
		//     break;
		// case PARAM_NEXT_NOTE_FREQ:
		//     g_module.next_note_freq = value;
		//     break;
		// case PARAM_TICKS_TIL_NEXT:
		//     g_module.ticks_til_next = value;
		//     break;

		// case PARAM_CUTOFF:
		//     g_module.cutoff = value;
		//     Aleph_MonoVoice_set_cutoff(&g_module.voice[0], value);
		//     break;
		// case PARAM_RESONANCE:
		//     g_module.resonance = value;
		//     Aleph_MonoVoice_set_res(&g_module.voice[0], value);
		//     break;
		
		case TRANSPORT_TEMPO: {
			dsp_playhead_set_tempo((uint32_t)value);
		} break;
		case TRANSPORT_PLAY: {
			dsp_playhead_set_pos((uint32_t)value);
			dsp_playhead_playstop(true);
		} break;
		case TRANSPORT_STOP: {
			dsp_playhead_set_pos((uint32_t)value);
			dsp_playhead_playstop(false);
		} break;

		case PARAM_IFX0_PARAM0: {
			flanger_set_param(g_flanger, 0, value);
		} break;
		case PARAM_IFX0_PARAM1: {
			flanger_set_param(g_flanger, 1, value);
		} break;
		case PARAM_IFX1_PARAM0: {
			ott_set_param(g_ott, 0, value);
		} break;
		case PARAM_IFX1_PARAM1: {
			ott_set_param(g_ott, 1, value);
		} break;

	}
}

/**
 * @brief   Get parameter.
 *
 * @param[in]   param_index Index of parameter to get.
 *
 * @return      value       Value of parameter.
 */
int32_t module_get_param(uint16_t param_index) {

	int32_t value = 0;

	switch (param_index) {
	default:
		break;
	}

	return value;
}

/**
 * @brief   Get number of parameters.
 *
 * @return  Number of parameters
 */
uint32_t module_get_param_count(void) { return 0; }

/**
 * @brief   Get name of parameter at index.
 *
 * @param[in]   param_index     Index pf parameter.
 * @param[out]  text            Buffer to store string.
 *                              Must provide 'MAX_PARAM_NAME_LENGTH'
 *                              bytes of storage.
 */
void module_get_param_name(uint16_t param_index, char *text) {

	switch (param_index) {

	default:
		copy_string(text, "Unknown", MAX_PARAM_NAME_LENGTH);
		break;
	}
}


/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
