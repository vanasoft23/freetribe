#ifndef PARAM_DEFS_H
#define PARAM_DEFS_H

// SPI messages
#define PARAM_NOTE_FREQ      2
#define PARAM_GATE_TICKS     3
#define PARAM_NEXT_NOTE_FREQ 4
#define PARAM_TICKS_TIL_NEXT 5 // the number of DSP frames it takes to get to the next note after the current one's gate ends
#define PARAM_CUTOFF         10
#define PARAM_RESONANCE      11
#define PARAM_IFX0_PARAM0    15
#define PARAM_IFX0_PARAM1    16
#define PARAM_IFX1_PARAM0    17
#define PARAM_IFX1_PARAM1    18
#define TRANSPORT_PLAY       50
#define TRANSPORT_STOP       51
#define TRANSPORT_TEMPO      52

#endif