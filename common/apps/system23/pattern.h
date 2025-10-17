#ifndef PATTERN_H
#define PATTERN_H

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>

/*----- Macros -------------------------------------------------------*/

#define GATE_FULL     255
#define GATE_HALF     127
#define GATE_QTER      63

/*----- Typedefs -----------------------------------------------------*/

typedef struct __attribute__((packed, aligned(4))) {
    uint8_t     gate;
    uint8_t     note;
    bool        slide;
    bool        accent;
} t_step;

typedef struct __attribute__((packed, aligned(4))) {
    t_step      steps[64];
    uint8_t     effect_0_id;
    uint8_t     effect_1_id;
} t_part;

typedef struct __attribute__((packed, aligned(4))) {
    t_part      parts[16];
} t_pattern;

/*----- Functions ----------------------------------------------------*/

static inline bool pattern_has_note(t_pattern *ptn, int part_idx, int step_idx) {
    return (ptn->parts[part_idx].steps[step_idx].gate > 0);
}

void pattern_place_note(t_pattern *ptn, int part_idx, int step_idx);

#endif