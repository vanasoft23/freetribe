
/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pattern.h"
#include "playback.h"
#include "view.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Variables ----------------------------------------------------*/

static u8  s_bar      = 0;
static u8  s_step     = 0;

static TickType_t s_ticks_per_step;
static TickType_t s_ticks;
static TickType_t s_old_ticks;

/*----- Static function declarations ---------------------------------*/


/*----- Extern function implementations ------------------------------*/

void playback_init(int initial_bpm)
{
	
	playback_set_tempo(initial_bpm);
	s_old_ticks = xTaskGetTickCount(); // Reset beat timer.

}

void playback_set_tempo(int bpm)
{

	s_ticks_per_step = pdMS_TO_TICKS(60000 / (bpm * 4));

}



void playback_process(void)
{

	TickType_t now = xTaskGetTickCount();
	s_ticks += (TickType_t)(now - s_old_ticks);
	s_old_ticks = now;

	// while (s_ticks > s_ticks_per_step)
{
	//     s_ticks -= s_ticks_per_step;
		
	//     // Play note
	//     t_pattern *ptn = ptn_ptr();
	//     u8 ptn_step = 16 * s_bar + s_step;
	//     if (pattern_has_note(ptn, g_part_idx, ptn_step))
{
	//         // ft_set_module_param(0, PARAM_NOTE_FREQ, NOTE_FREQS[g_sequencer_steps[ptn_step].note]);
	//         // ft_set_module_param(0, PARAM_GATE_TICKS, g_sequencer_steps[ptn_step].gate);
	//         // u8 next_step = (ptn_step+1)%(sizeof(g_sequencer_steps)/sizeof(t_step));
	//         // s32 ticks_til_next_gate = GATE_4TH - g_sequencer_steps[ptn_step].gate;
	//         // ft_set_module_param(0, PARAM_TICKS_TIL_NEXT, ticks_til_next_gate);
	//         // ft_set_module_param(0, PARAM_NEXT_NOTE_FREQ, NOTE_FREQS[g_sequencer_steps[next_step].note]);
	//     }

	//     s_step++;
	//     if (s_step >= 16)
{
	//         s_step = 0;
	//         s_bar++;
	//         if (s_bar > 3)
{
	//             s_bar = 0;
	//         }
	//     }

	//     // // if the pattern is smaller than 64, we gotta reset the sequencer position
	//     // g_total_steps++;
	//     // if (g_total_steps >= g_sequencer_num_steps)
{
	//     //     g_total_steps = 0;
	//     //     s_bar = 0;
	//     //     s_step = 0;
	//     // }

	// }
}

/*----- End of file --------------------------------------------------*/
