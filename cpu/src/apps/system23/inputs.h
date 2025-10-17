#ifndef INPUTS_H
#define INPUTS_H

#define BUTTON_RECORD       0x00
#define BUTTON_STOP         0x01
#define BUTTON_PLAY         0x02
#define BUTTON_TAP          0x03
#define BUTTON_GATE_ARP     0x04
#define BUTTON_TOUCH_SCALE  0x05
#define BUTTON_MASTER_FX    0x06
#define BUTTON_MFX_HOLD     0x07
#define BUTTON_LEFT         0x08
#define BUTTON_ENTER        0x09
#define BUTTON_SHIFT        0x0A
#define BUTTON_PART_PREV    0x0B
#define BUTTON_RIGHT        0x0C
#define BUTTON_EXIT         0x0D
#define BUTTON_WRITE        0x0E
#define BUTTON_PART_NEXT    0x0F
#define BUTTON_PART_MUTE    0x10
#define BUTTON_PART_ERASE   0x11
#define BUTTON_LPF          0x12
#define BUTTON_TRIGGER      0x13
#define BUTTON_HPF          0x14
#define BUTTON_SEQUENCER    0x15
#define BUTTON_BPF          0x16
#define BUTTON_KEYBOARD     0x17
#define BUTTON_CHORD        0x18
#define BUTTON_STEP_JUMP    0x19
#define BUTTON_MFX_SEND     0x1A
#define BUTTON_PATTERN_SET  0x1B
#define BUTTON_BAR_0        0x1C
#define BUTTON_BAR_1        0x1D
#define BUTTON_BAR_2        0x1E
#define BUTTON_BAR_3        0x1F
#define BUTTON_AMP_EG       0x20
#define BUTTON_IFX_ON       0x21

#define KNOB_LEVEL          0x00
#define KNOB_PAN            0x01
#define KNOB_PITCH          0x02
#define KNOB_RESONANCE      0x03
#define KNOB_EG             0x04
#define KNOB_MOD_DEPTH      0x05
#define KNOB_ATTACK         0x06
#define KNOB_IFX_EDIT       0x07
#define KNOB_DECAY          0x08
#define KNOB_OSC_EDIT       0x09
#define KNOB_MOD_SPEED      0x0A

#define ENCODER_OSC         0x01
#define ENCODER_CUTOFF      0x02
#define ENCODER_MOD         0x03





// @TODO: move to some DSP common header
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



#endif