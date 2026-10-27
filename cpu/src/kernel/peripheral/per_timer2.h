/**
 * @file    per_timer2.h
 * @brief   AM1802 Timer Plus configuration and control.
 */

#pragma once

#include "ft.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIMER0_BASE                     0x01C20000UL
#define TIMER1_BASE                     0x01C21000UL
#define TIMER2_BASE                     0x01F0C000UL
#define TIMER3_BASE                     0x01F0D000UL

// #define TIMER_PERIOD_1s                 0x8F0D17Fu // Assumes a 150 MHz input T2 T3 PLL0_AUXCLK.
// #define TIMER_PERIOD_500us              0x124F7u // Assumes a 150 MHz input T2 T3 PLL0_AUXCLK.
// #define TIMER_PERIOD_1ms                0x5DBFu  // Assumes a 24 MHz input T0 T1 ASYNC3.

// Callbacks run in IRQ context after acknowledgement. Do not block or reinitialize.
typedef void (*timer_callback_t)(void *context);
typedef void (*timer_capture_callback_t)(u32 count, void *context);

typedef enum {
	TIMER_MODE_64_BIT,
	TIMER_MODE_32_BIT_UNCHAINED,
	TIMER_MODE_32_BIT_CHAINED,
	TIMER_MODE_WATCHDOG,            // TIMER1 only; cannot stop after activation.
} timer_mode_t;

typedef enum {
	TIMER_SOURCE_INTERNAL,
	TIMER_SOURCE_EXTERNAL,
} timer_source_t;

typedef enum {
	TIMER_OPER_ONE_TIME,
	TIMER_OPER_CONTINUOUS,
	TIMER_OPER_RELOAD,
} timer_operation_t;

typedef enum {
	TIMER_HALF_12   = 1,
	TIMER_HALF_34   = 2,
	TIMER_HALF_BOTH = 3,
} timer_half_t;

typedef enum {
	TIMER_CAPTURE_DISABLED,
	TIMER_CAPTURE_RISING,
	TIMER_CAPTURE_FALLING,
	TIMER_CAPTURE_BOTH,
} timer_capture_edge_t;

typedef enum {
	TIMER_EVENT_PERIOD12    = 1u << 0,
	TIMER_EVENT_PERIOD34    = 1u << 1,
	TIMER_EVENT_CAPTURE12   = 1u << 2,
	TIMER_EVENT_COMPARE0    = 1u << 8,
	TIMER_EVENT_COMPARE1    = 1u << 9,
	TIMER_EVENT_COMPARE2    = 1u << 10,
	TIMER_EVENT_COMPARE3    = 1u << 11,
	TIMER_EVENT_COMPARE4    = 1u << 12,
	TIMER_EVENT_COMPARE5    = 1u << 13,
	TIMER_EVENT_COMPARE6    = 1u << 14,
	TIMER_EVENT_COMPARE7    = 1u << 15,
	TIMER_EVENT_COMPARE_ALL = 0xFF00u,
	TIMER_EVENT_ALL         = 0xFF07u,
} timer_event_t;

// index must be 0-7.
#define TIMER_EVENT_COMPARE(index) ((u32)TIMER_EVENT_COMPARE0 << (index))

typedef struct {
	timer_operation_t operation;
	u64               period;       // Raw PRD/REL value; normally N - 1 for N ticks.
	timer_callback_t  on_period;
	void             *context;
	bool              period_event; // Enable period/DMA events without a callback.
} timer_counter_config_t;

typedef struct {
	timer_capture_edge_t     edge;
	timer_capture_callback_t callback;
	void                    *context;
	bool                     event; // Enable capture/DMA events without a callback.
} timer_capture_config_t;

typedef struct {
	u32              value;         // Absolute TIM12 count; zero disables matches.
	timer_callback_t callback;
	void            *context;
} timer_compare_config_t;

typedef struct {
	timer_mode_t           mode;
	timer_source_t         source;      // External input feeds TIM12; TIM34 is internal.
	u8                     int_channel; // 0 selects IRQ channel 12; otherwise 2-31.
	timer_counter_config_t counter12;
	timer_counter_config_t counter34;
	timer_capture_config_t capture12;
	timer_compare_config_t compare[8];
} timer_config_t;

// Convert microseconds to raw PRD/REL, rounded to the nearest input tick.
// clock_hz includes any prescaling. Requires at least two rounded ticks.
// Returns false for invalid arguments; output is unchanged on failure.
bool per_timer2_calc_period(u32 clock_hz, u32 duration_us, u64 *period);

// Configure the whole peripheral and leave it stopped. REL initially equals PRD.
// Caller supplies clock/power, pinmux, and EDMA routing; the driver owns its IRQs.
// Use privileged task/IRQ context and serialize initialization with other users.
//
// counter12.period: PRD12 in 32-bit modes; complete 64-bit period in 64-bit/watchdog.
// counter34.period: PRD34 in unchained mode; prescaler PRD34 in chained mode.
// counter34.operation/on_period/period_event apply only to unchained mode.
// In 64-bit/watchdog mode counter34.period is unused. TIM34's 4-bit prescaler
// remains divide-by-1. Zero periods may configure an unused counter but cannot run.
//
// Capture requires unchained TIM12, internal clock, and CONTINUOUS or RELOAD.
// Capture restarts TIM12 on each selected edge; only the latest count is retained.
// CMP interrupts/DMA are available on P2/P3 TIM12 in unchained mode.
// Callbacks enable CPU IRQ delivery; event enables also support polling/DMA alone.
// P2/P3 share period/capture IRQs: callback-less events can be acknowledged by a
// sibling's ISR. CMP sources have independent IRQs.
//
// Watchdog requires TIMER1, internal clock, CONTINUOUS, no callbacks/events.
// start(BOTH) activates it; only service_watchdog() may modify it afterwards.
// Invalid configuration or an already active watchdog returns false.
bool per_timer2_init(u32 base, const timer_config_t *config);

// Select individual halves or BOTH in unchained mode; paired modes require BOTH.
// start resumes; stop preserves count; reset stops and zeros; restart zeros/starts.
// Stop/reset/restart acknowledge the selected counters and their pending CMPs.
// External clocks need one full input period between stop and start. RESTART of
// external TIM12 returns false: reset, wait one input period, then start instead.
bool per_timer2_start(u32 base, timer_half_t half);
bool per_timer2_stop(u32 base, timer_half_t half);
bool per_timer2_reset(u32 base, timer_half_t half);
bool per_timer2_restart(u32 base, timer_half_t half);

// RELOAD operation only; changes REL for a later cycle without changing PRD now.
// Select 12 or 34 individually in unchained/chained mode; BOTH in 64-bit mode.
// In chained mode 34 selects the prescaler period. Finish paired writes before
// the cycle boundary when both reload registers must change together.
bool per_timer2_reload(u32 base, timer_half_t half, u64 period);
bool per_timer2_service_watchdog(u32 base);

// P2/P3 unchained only, index 0-7. Targets can change while counting.
// Nonzero updates preserve already pending matches. Zero disables/clears the slot.
// Matches repeat whenever TIM12 reaches the target; callbacks are set during init.
bool per_timer2_set_compare(u32 base, u8 index, u32 count);

u32 per_timer2_get_count12(u32 base);
u32 per_timer2_get_count34(u32 base);
// Meaningful in 64-bit/watchdog mode; reads low then latched high.
u64 per_timer2_get_count64(u32 base);
u32 per_timer2_get_capture(u32 base);

// Return latched flags, not an event queue; CMP flags come from raw AINTC status.
// clear_events preserves enable bits and unselected events. It clears timer flags
// in the peripheral; the ISR acknowledges its AINTC route. CMP flags are cleared
// directly in AINTC. Unsupported event bits in a valid mask are ignored.
u32  per_timer2_get_events(u32 base);
bool per_timer2_clear_events(u32 base, u32 events);

#ifdef __cplusplus
}
#endif
