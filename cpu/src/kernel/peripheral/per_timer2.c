/**
 * @file    per_timer2.c
 * @brief   Driver for the four AM1802 Timer Plus peripherals.
 * @author  vanasoft23
 */

#include "per_timer2.h"

#include <am18x_map.h>

#include "per_aintc.h"

#define TIMER_DEFAULT_INT_CHANNEL 12
#define TIMER_ENABLE_BITS (INTCTLSTAT_PRDINTEN12_MASK | INTCTLSTAT_EVTINTEN12_MASK \
                         | INTCTLSTAT_PRDINTEN34_MASK | INTCTLSTAT_EVTINTEN34_MASK)
#define TIMER_STATUS_BITS (INTCTLSTAT_PRDINTSTAT12_MASK | INTCTLSTAT_EVTINTSTAT12_MASK \
                         | INTCTLSTAT_PRDINTSTAT34_MASK | INTCTLSTAT_EVTINTSTAT34_MASK)

//--------------------------------------------------------------------

static const u32 BASES[] = { TIMER0_BASE, TIMER1_BASE, TIMER2_BASE, TIMER3_BASE };

static const u32 MODES[] = {
	[TIMER_MODE_64_BIT]           = TGCR_TIMMODE_64bit,
	[TIMER_MODE_32_BIT_UNCHAINED] = TGCR_TIMMODE_unchained,
	[TIMER_MODE_32_BIT_CHAINED]   = TGCR_TIMMODE_chained,
	[TIMER_MODE_WATCHDOG]         = TGCR_TIMMODE_watchdog,
};

static const u32 OPERATIONS12[] = {
	[TIMER_OPER_ONE_TIME]         = TCR_ENAMODE12_once,
	[TIMER_OPER_CONTINUOUS]       = TCR_ENAMODE12_enabled,
	[TIMER_OPER_RELOAD]           = TCR_ENAMODE12_reload,
};

static const u32 OPERATIONS34[] = {
	[TIMER_OPER_ONE_TIME]         = TCR_ENAMODE34_once,
	[TIMER_OPER_CONTINUOUS]       = TCR_ENAMODE34_enabled,
	[TIMER_OPER_RELOAD]           = TCR_ENAMODE34_reload,
};

static const u32 SOURCES[] = {
	[TIMER_SOURCE_INTERNAL]      = TCR_CLKSRC12_internal,
	[TIMER_SOURCE_EXTERNAL]      = TCR_CLKSRC12_external,
};

static const u32 CAPTURE_EDGES[] = {
	[TIMER_CAPTURE_DISABLED]     = 0,
	[TIMER_CAPTURE_RISING]       = TCR_CAPEVTMODE12_rising,
	[TIMER_CAPTURE_FALLING]      = TCR_CAPEVTMODE12_falling,
	[TIMER_CAPTURE_BOTH]         = TCR_CAPEVTMODE12_both,
};

// P0/P1 have separate interrupts; P2/P3 share their period/capture interrupt.
static const u8 SYSINT[][2] = {
	{AINTC_T64P0_TINT12, AINTC_T64P0_TINT34},
	{AINTC_T64P1_TINT12, AINTC_T64P1_TINT34},
	{AINTC_T64P2_ALL, AINTC_T64P2_ALL},
	{AINTC_T64P3_ALL, AINTC_T64P3_ALL},
};

static const u8 COMPARE_SYSINT[] = { AINTC_T64P2_CMPINT0, AINTC_T64P3_CMPINT0 };

static timer_config_t g_config[4];
static bool g_initialized[4];

static int  _timer_index(u32 base);
static int  _control_index(u32 base, timer_half_t half);
static bool _valid_period(u8 instance, timer_half_t half);
static void _start(u8 instance, timer_half_t half);
static void _stop(u8 instance, timer_half_t half, bool reset);
static inline u32  _irq_save(void);
static inline void _irq_restore(u32 state);

static void _timer0_isr12(void);
static void _timer0_isr34(void);
static void _timer1_isr12(void);
static void _timer1_isr34(void);
static void _timer2_isr(void);
static void _timer3_isr(void);
static void _timer0_capture_isr(void);
static void _timer1_capture_isr(void);
static void _timer2_capture_isr(void);
static void _timer3_capture_isr(void);

typedef void (*timer_irq_t)(void);

static const timer_irq_t PERIOD_ISRS[][2] = {
	{_timer0_isr12, _timer0_isr34},
	{_timer1_isr12, _timer1_isr34},
	{_timer2_isr, _timer2_isr},
	{_timer3_isr, _timer3_isr},
};

static const timer_irq_t CAPTURE_ISRS[] = {
	_timer0_capture_isr, _timer1_capture_isr, _timer2_capture_isr, _timer3_capture_isr,
};

// Each compare has its own vector. Fixed addresses avoid a dispatcher or scan.
#define DEFINE_COMPARE_ISR(instance, slot) \
	static void _timer##instance##_cmp##slot(void) \
	{ \
		AINTC->SIxR[AINTC_IDX_CLR] = COMPARE_SYSINT[instance - 2] + slot; \
		g_config[instance].compare[slot].callback(g_config[instance].compare[slot].context); \
	}

DEFINE_COMPARE_ISR(2, 0)
DEFINE_COMPARE_ISR(2, 1)
DEFINE_COMPARE_ISR(2, 2)
DEFINE_COMPARE_ISR(2, 3)
DEFINE_COMPARE_ISR(2, 4)
DEFINE_COMPARE_ISR(2, 5)
DEFINE_COMPARE_ISR(2, 6)
DEFINE_COMPARE_ISR(2, 7)
DEFINE_COMPARE_ISR(3, 0)
DEFINE_COMPARE_ISR(3, 1)
DEFINE_COMPARE_ISR(3, 2)
DEFINE_COMPARE_ISR(3, 3)
DEFINE_COMPARE_ISR(3, 4)
DEFINE_COMPARE_ISR(3, 5)
DEFINE_COMPARE_ISR(3, 6)
DEFINE_COMPARE_ISR(3, 7)

#undef DEFINE_COMPARE_ISR

static const timer_irq_t COMPARE_ISRS[][8] = {
	{_timer2_cmp0, _timer2_cmp1, _timer2_cmp2, _timer2_cmp3,
	 _timer2_cmp4, _timer2_cmp5, _timer2_cmp6, _timer2_cmp7},
	{_timer3_cmp0, _timer3_cmp1, _timer3_cmp2, _timer3_cmp3,
	 _timer3_cmp4, _timer3_cmp5, _timer3_cmp6, _timer3_cmp7},
};

//--------------------------------------------------------------------

bool per_timer2_calc_period(u32 clock_hz, u32 duration_us, u64 *period)
{
	if (clock_hz == 0 || period == NULL)
		return false;

	u64 ticks = ((u64)clock_hz * duration_us + 500000ULL) / 1000000ULL;
	if (ticks < 2)
		return false;

	*period = ticks - 1;
	return true;
}

bool per_timer2_init(u32 base, const timer_config_t *config)
{
	int instance = _timer_index(base);
	if (instance < 0 || config == NULL)
		return false;

	// Validate the complete configuration before changing any registers.
	if ((unsigned)config->mode > TIMER_MODE_WATCHDOG)
		return false;
	if ((unsigned)config->source > TIMER_SOURCE_EXTERNAL)
		return false;
	if ((unsigned)config->counter12.operation > TIMER_OPER_RELOAD)
		return false;
	if ((unsigned)config->capture12.edge > TIMER_CAPTURE_BOTH)
		return false;

	bool unchained = config->mode == TIMER_MODE_32_BIT_UNCHAINED;
	bool paired64  = config->mode == TIMER_MODE_64_BIT || config->mode == TIMER_MODE_WATCHDOG;
	bool capture   = config->capture12.edge != TIMER_CAPTURE_DISABLED;
	u8 channel = config->int_channel ? config->int_channel : TIMER_DEFAULT_INT_CHANNEL;

	if (channel < 2 || channel > 31)
		return false;
	if (!paired64 && (config->counter12.period > UINT32_MAX
	                 || config->counter34.period > UINT32_MAX))
		return false;
	if (unchained && (unsigned)config->counter34.operation > TIMER_OPER_RELOAD)
		return false;
	if (!unchained && (config->counter34.on_period || config->counter34.period_event))
		return false;

	if (capture) {
		if (!unchained || config->source != TIMER_SOURCE_INTERNAL)
			return false;
		if (config->counter12.operation == TIMER_OPER_ONE_TIME)
			return false;
	} else if (config->capture12.callback || config->capture12.event) {
		return false;
	}

	for (int i = 0; i < 8; i++) {
		if ((!unchained || instance < 2) && (config->compare[i].value
		                                  || config->compare[i].callback))
			return false;
	}

	if (config->mode == TIMER_MODE_WATCHDOG) {
		if (base != TIMER1_BASE || config->source != TIMER_SOURCE_INTERNAL)
			return false;
		if (config->counter12.operation != TIMER_OPER_CONTINUOUS)
			return false;
		if (config->counter12.on_period || config->counter12.period_event)
			return false;
	}

	TIMER_con_t *con = (TIMER_con_t*)base;
	u32 irq_state = _irq_save();
	if (con->WDTCR & WDTCR_WDEN_MASK) {
		_irq_restore(irq_state);
		return false;
	}

	// Stop and hold both halves in reset before writing protected registers.
	con->TCR  &= ~(TCR_ENAMODE12_MASK | TCR_ENAMODE34_MASK);
	con->TGCR &= ~(TGCR_TIM12RS_MASK | TGCR_TIM34RS_MASK);
	con->INTCTLSTAT = TIMER_STATUS_BITS;

	// Remove all routes owned by the previous configuration.
	per_aintc_unregister_int(SYSINT[instance][0]);
	if (instance < 2)
		per_aintc_unregister_int(SYSINT[instance][1]);
	if (instance >= 2) {
		for (int i = 0; i < 8; i++)
			per_aintc_unregister_int(COMPARE_SYSINT[instance - 2] + i);
	}

	// Select timer mode and clock. GPIO mode must be disabled.
	con->GPINTGPEN = 0;
	con->EMUMGT   = FIELD_SET(con->EMUMGT, EMUMGT_FREE_MASK, EMUMGT_FREE_run);
	con->TGCR     = TGCR_PLUSEN_available | MODES[config->mode];
	con->TCR      = SOURCES[config->source];
	if (capture)
		con->TCR |= TCR_CAPMODE12_capture | CAPTURE_EDGES[config->capture12.edge];

	// Set counts, periods, and initial automatic reload values.
	con->TIMx[0] = 0;
	con->TIMx[1] = 0;
	con->PRDx[0] = (u32)config->counter12.period;
	con->PRDx[1] = paired64 ? (u32)(config->counter12.period >> 32)
	                       : (u32)config->counter34.period;
	con->RELx[0] = con->PRDx[0];
	con->RELx[1] = con->PRDx[1];
	for (int i = 0; i < 8; i++)
		con->CMPx[i] = config->compare[i].value;

	// Save callbacks before enabling CPU interrupt delivery.
	g_config[instance]      = *config;
	g_initialized[instance] = true;

	bool callback12 = config->counter12.on_period || config->capture12.callback;
	bool callback34 = unchained && config->counter34.on_period;
	bool capture_events = config->capture12.callback || config->capture12.event;
	timer_irq_t isr12 = capture_events ? CAPTURE_ISRS[instance] : PERIOD_ISRS[instance][0];

	if (callback12 || (instance >= 2 && callback34))
		per_aintc_register_int(channel, SYSINT[instance][0], isr12);
	if (instance < 2 && callback34)
		per_aintc_register_int(channel, SYSINT[instance][1], PERIOD_ISRS[instance][1]);
	if (instance >= 2) {
		for (int i = 0; i < 8; i++) {
			if (config->compare[i].callback)
				per_aintc_register_int(channel, COMPARE_SYSINT[instance - 2] + i,
				                      COMPARE_ISRS[instance - 2][i]);
		}
	}

	// Event generation is independent of CPU interrupt routing.
	u32 reg = 0;
	if (config->counter12.on_period || config->counter12.period_event)
		reg |= INTCTLSTAT_PRDINTEN12_enable;
	if (unchained && (config->counter34.on_period || config->counter34.period_event))
		reg |= INTCTLSTAT_PRDINTEN34_enable;
	if (capture_events)
		reg |= INTCTLSTAT_EVTINTEN12_enable;
	con->INTCTLSTAT = reg;

	// Release reset. ENAMODE stays disabled until start().
	con->TGCR |= TGCR_TIM12RS_none | TGCR_TIM34RS_none;
	_irq_restore(irq_state);
	return true;
}

//--------------------------------------------------------------------

bool per_timer2_start(u32 base, timer_half_t half)
{
	int instance = _control_index(base, half);
	if (instance < 0)
		return false;

	TIMER_con_t *con = (TIMER_con_t*)base;
	u32 irq_state = _irq_save();
	if (!_valid_period(instance, half)) {
		_irq_restore(irq_state);
		return false;
	}

	if (g_config[instance].mode == TIMER_MODE_WATCHDOG) {
		if (con->WDTCR & WDTCR_WDEN_MASK) {
			_irq_restore(irq_state);
			return false;
		}
		con->WDTCR = WDTCR_WDEN_enable | WDTCR_WDKEY_VAL(0xA5C6UL);
		con->WDTCR = WDTCR_WDEN_enable | WDTCR_WDKEY_VAL(0xDA7EUL);
	} else {
		_start(instance, half);
	}

	_irq_restore(irq_state);
	return true;
}

bool per_timer2_stop(u32 base, timer_half_t half)
{
	int instance = _control_index(base, half);
	if (instance < 0 || g_config[instance].mode == TIMER_MODE_WATCHDOG)
		return false;

	u32 irq_state = _irq_save();
	_stop(instance, half, false);
	_irq_restore(irq_state);
	return true;
}

bool per_timer2_reset(u32 base, timer_half_t half)
{
	int instance = _control_index(base, half);
	if (instance < 0 || g_config[instance].mode == TIMER_MODE_WATCHDOG)
		return false;

	u32 irq_state = _irq_save();
	_stop(instance, half, true);
	_irq_restore(irq_state);
	return true;
}

bool per_timer2_restart(u32 base, timer_half_t half)
{
	int instance = _control_index(base, half);
	if (instance < 0 || g_config[instance].mode == TIMER_MODE_WATCHDOG)
		return false;

	// An atomic restart cannot guarantee the delay required by an external clock.
	if (g_config[instance].source == TIMER_SOURCE_EXTERNAL && (half & TIMER_HALF_12))
		return false;

	u32 irq_state = _irq_save();
	if (!_valid_period(instance, half)) {
		_irq_restore(irq_state);
		return false;
	}
	_stop(instance, half, true);
	_start(instance, half);
	_irq_restore(irq_state);
	return true;
}

bool per_timer2_reload(u32 base, timer_half_t half, u64 period)
{
	int instance = _timer_index(base);
	if (instance < 0 || !g_initialized[instance] || period == 0)
		return false;

	timer_config_t *config = &g_config[instance];
	if (config->mode == TIMER_MODE_WATCHDOG)
		return false;

	TIMER_con_t *con = (TIMER_con_t*)base;
	if (config->mode == TIMER_MODE_64_BIT) {
		if (half != TIMER_HALF_BOTH || config->counter12.operation != TIMER_OPER_RELOAD)
			return false;
	} else {
		if ((half != TIMER_HALF_12 && half != TIMER_HALF_34) || period > UINT32_MAX)
			return false;
		timer_operation_t operation = config->counter12.operation;
		if (config->mode == TIMER_MODE_32_BIT_UNCHAINED && half == TIMER_HALF_34)
			operation = config->counter34.operation;
		if (operation != TIMER_OPER_RELOAD)
			return false;
	}

	u32 irq_state = _irq_save();
	if (half == TIMER_HALF_BOTH) {
		con->RELx[0] = (u32)period;
		con->RELx[1] = (u32)(period >> 32);
	} else {
		con->RELx[half == TIMER_HALF_34] = (u32)period;
	}
	_irq_restore(irq_state);
	return true;
}

bool per_timer2_service_watchdog(u32 base)
{
	int instance = _timer_index(base);
	if (instance < 0 || !g_initialized[instance]
	 || g_config[instance].mode != TIMER_MODE_WATCHDOG)
		return false;

	TIMER_con_t *con = (TIMER_con_t*)base;
	u32 irq_state = _irq_save();
	if (!(con->WDTCR & WDTCR_WDEN_MASK)) {
		_irq_restore(irq_state);
		return false;
	}
	con->WDTCR = WDTCR_WDEN_enable | WDTCR_WDKEY_VAL(0xA5C6UL);
	con->WDTCR = WDTCR_WDEN_enable | WDTCR_WDKEY_VAL(0xDA7EUL);
	_irq_restore(irq_state);
	return true;
}

bool per_timer2_set_compare(u32 base, u8 index, u32 count)
{
	int instance = _timer_index(base);
	if (instance < 2 || !g_initialized[instance] || index >= 8)
		return false;
	if (g_config[instance].mode != TIMER_MODE_32_BIT_UNCHAINED)
		return false;

	TIMER_con_t *con = (TIMER_con_t*)base;
	if (count != 0) {
		con->CMPx[index] = count;
	} else {
		// Disable matches before discarding the pending event.
		u32 irq_state = _irq_save();
		con->CMPx[index] = 0;
		AINTC->SIxR[AINTC_IDX_CLR] = COMPARE_SYSINT[instance - 2] + index;
		_irq_restore(irq_state);
	}
	return true;
}

//--------------------------------------------------------------------

u32 per_timer2_get_count12(u32 base)
{
	if (_timer_index(base) < 0)
		return 0;
	return ((TIMER_con_t*)base)->TIMx[0];
}

u32 per_timer2_get_count34(u32 base)
{
	if (_timer_index(base) < 0)
		return 0;
	return ((TIMER_con_t*)base)->TIMx[1];
}

u64 per_timer2_get_count64(u32 base)
{
	if (_timer_index(base) < 0)
		return 0;

	TIMER_con_t *con = (TIMER_con_t*)base;
	u32 irq_state = _irq_save();
	// Reading TIM12 snapshots TIM34; keep both reads in order and uninterrupted.
	u32 low  = con->TIMx[0];
	u32 high = con->TIMx[1];
	_irq_restore(irq_state);
	return ((u64)high << 32) | low;
}

u32 per_timer2_get_capture(u32 base)
{
	if (_timer_index(base) < 0)
		return 0;
	return ((TIMER_con_t*)base)->CAPx[0];
}

u32 per_timer2_get_events(u32 base)
{
	int instance = _timer_index(base);
	if (instance < 0 || !g_initialized[instance])
		return 0;

	u32 reg = ((TIMER_con_t*)base)->INTCTLSTAT;
	u32 events = 0;
	if (reg & INTCTLSTAT_PRDINTSTAT12_MASK)
		events |= TIMER_EVENT_PERIOD12;
	if (reg & INTCTLSTAT_PRDINTSTAT34_MASK)
		events |= TIMER_EVENT_PERIOD34;
	if (reg & INTCTLSTAT_EVTINTSTAT12_MASK)
		events |= TIMER_EVENT_CAPTURE12;

	if (instance >= 2) {
		// All eight CMP sources for either peripheral fit in one raw status word.
		u8 sysint = COMPARE_SYSINT[instance - 2];
		u32 pending = AINTC->SRSRx[SXXRx_WR_X(sysint)] >> (sysint & 31);
		events |= (pending & 0xFFu) * TIMER_EVENT_COMPARE0;
	}
	return events;
}

bool per_timer2_clear_events(u32 base, u32 events)
{
	int instance = _timer_index(base);
	if (instance < 0 || !g_initialized[instance] || (events & ~TIMER_EVENT_ALL))
		return false;
	if (g_config[instance].mode == TIMER_MODE_WATCHDOG)
		return false;

	u32 status = 0;
	if (events & TIMER_EVENT_PERIOD12)
		status |= INTCTLSTAT_PRDINTSTAT12_MASK;
	if (events & TIMER_EVENT_PERIOD34)
		status |= INTCTLSTAT_PRDINTSTAT34_MASK;
	if (events & TIMER_EVENT_CAPTURE12)
		status |= INTCTLSTAT_EVTINTSTAT12_MASK;

	u32 irq_state = _irq_save();
	TIMER_con_t *con = (TIMER_con_t*)base;
	con->INTCTLSTAT = (con->INTCTLSTAT & TIMER_ENABLE_BITS) | status;
	if (instance >= 2) {
		for (int i = 0; i < 8; i++) {
			if (events & TIMER_EVENT_COMPARE(i))
				AINTC->SIxR[AINTC_IDX_CLR] = COMPARE_SYSINT[instance - 2] + i;
		}
	}
	// Do not clear the shared timer route here: another half may still be pending.
	_irq_restore(irq_state);
	return true;
}

//--------------------------------------------------------------------

static int _timer_index(u32 base)
{
	for (int i = 0; i < 4; i++) {
		if (BASES[i] == base)
			return i;
	}
	return -1;
}

static int _control_index(u32 base, timer_half_t half)
{
	int instance = _timer_index(base);
	if (instance < 0 || !g_initialized[instance])
		return -1;
	if (half < TIMER_HALF_12 || half > TIMER_HALF_BOTH)
		return -1;
	if (g_config[instance].mode != TIMER_MODE_32_BIT_UNCHAINED && half != TIMER_HALF_BOTH)
		return -1;
	return instance;
}

static bool _valid_period(u8 instance, timer_half_t half)
{
	TIMER_con_t *con = (TIMER_con_t*)BASES[instance];
	timer_mode_t mode = g_config[instance].mode;
	if (mode == TIMER_MODE_64_BIT || mode == TIMER_MODE_WATCHDOG)
		return con->PRDx[0] != 0 || con->PRDx[1] != 0;
	if ((half & TIMER_HALF_12) && con->PRDx[0] == 0)
		return false;
	if ((half & TIMER_HALF_34) && con->PRDx[1] == 0)
		return false;
	return true;
}

// Caller holds IRQs disabled; periods and selected halves have been validated.
static void _start(u8 instance, timer_half_t half)
{
	TIMER_con_t *con = (TIMER_con_t*)BASES[instance];
	timer_config_t *config = &g_config[instance];
	u32 mask   = 0;
	u32 enable = 0;
	if (half & TIMER_HALF_12) {
		mask   |= TCR_ENAMODE12_MASK;
		enable |= OPERATIONS12[config->counter12.operation];
	}
	if (half & TIMER_HALF_34) {
		mask |= TCR_ENAMODE34_MASK;
		if (config->mode == TIMER_MODE_32_BIT_UNCHAINED)
			enable |= OPERATIONS34[config->counter34.operation];
	}
	con->TCR = FIELD_SET(con->TCR, mask, enable);
}

// Shared by stop/reset/restart so acknowledgement and ordering stay identical.
static void _stop(u8 instance, timer_half_t half, bool reset)
{
	TIMER_con_t *con = (TIMER_con_t*)BASES[instance];
	u32 mask   = 0;
	u32 status = 0;
	if (half & TIMER_HALF_12) {
		mask   |= TCR_ENAMODE12_MASK;
		status |= INTCTLSTAT_PRDINTSTAT12_MASK | INTCTLSTAT_EVTINTSTAT12_MASK;
	}
	if (half & TIMER_HALF_34) {
		mask   |= TCR_ENAMODE34_MASK;
		status |= INTCTLSTAT_PRDINTSTAT34_MASK | INTCTLSTAT_EVTINTSTAT34_MASK;
	}

	con->TCR &= ~mask;
	if (reset) {
		if (half & TIMER_HALF_12)
			con->TIMx[0] = 0;
		if (half & TIMER_HALF_34)
			con->TIMx[1] = 0;
	}
	con->INTCTLSTAT = (con->INTCTLSTAT & TIMER_ENABLE_BITS) | status;
	if (instance >= 2 && (half & TIMER_HALF_12)) {
		for (int i = 0; i < 8; i++)
			AINTC->SIxR[AINTC_IDX_CLR] = COMPARE_SYSINT[instance - 2] + i;
	}
}

// @TODO: Move these CPU primitives to cpu/src/kernel/system/arm_irq.h.
// Preserve the caller's IRQ state, including calls made from an ISR.
static inline u32 _irq_save(void)
{
	u32 state, masked;
	__asm__ volatile (
		"mrs %0, cpsr\n\t"
		"orr %1, %0, #0x80\n\t"
		"msr cpsr_c, %1"
		: "=r" (state), "=&r" (masked)
		:
		: "memory", "cc"
	);
	return state;
}

///@TODO: move this!!!!
static inline void _irq_restore(u32 state)
{
	__asm__ volatile ("msr cpsr_c, %0" : : "r" (state) : "memory", "cc");
}

//--------------------------------------------------------------------

// Literal arguments resolve peripheral addresses, IRQs, and halves at compile time.
static inline __attribute__((always_inline)) u32
_timer_ack(u8 instance, timer_half_t half, u32 *captured)
{
	TIMER_con_t *con = (TIMER_con_t*)BASES[instance];
	AINTC->SIxR[AINTC_IDX_CLR] = SYSINT[instance][half == TIMER_HALF_34];

	u32 reg = con->INTCTLSTAT;
	u32 status_mask = 0;
	if (half & TIMER_HALF_12)
		status_mask |= INTCTLSTAT_PRDINTSTAT12_MASK;
	if (half & TIMER_HALF_34)
		status_mask |= INTCTLSTAT_PRDINTSTAT34_MASK;
	if (captured)
		status_mask |= INTCTLSTAT_EVTINTSTAT12_MASK;

	// Each status bit sits one bit above its enable bit.
	u32 enabled_status = (reg & TIMER_ENABLE_BITS) << 1;
	u32 pending = reg & enabled_status & status_mask;
	if (captured && (pending & INTCTLSTAT_EVTINTSTAT12_MASK))
		*captured = con->CAPx[0];

	// W1C: preserve every enable and acknowledge only the selected pending flags.
	con->INTCTLSTAT = (reg & TIMER_ENABLE_BITS) | pending;
	return pending;
}

static inline __attribute__((always_inline)) void
_timer_capture_irq(u8 instance, timer_half_t half)
{
	u32 captured = 0;
	u32 pending = _timer_ack(instance, half, &captured);
	timer_config_t *config = &g_config[instance];

	if ((pending & INTCTLSTAT_EVTINTSTAT12_MASK) && config->capture12.callback)
		config->capture12.callback(captured, config->capture12.context);
	if ((pending & INTCTLSTAT_PRDINTSTAT12_MASK) && config->counter12.on_period)
		config->counter12.on_period(config->counter12.context);
	if ((half & TIMER_HALF_34) && (pending & INTCTLSTAT_PRDINTSTAT34_MASK)
	 && config->counter34.on_period)
		config->counter34.on_period(config->counter34.context);
}

// Dedicated period-only IRQs are registered only when their callback is present.
static void _timer0_isr12(void)
{
	if (_timer_ack(0, TIMER_HALF_12, NULL))
		g_config[0].counter12.on_period(g_config[0].counter12.context);
}

static void _timer0_isr34(void)
{
	if (_timer_ack(0, TIMER_HALF_34, NULL))
		g_config[0].counter34.on_period(g_config[0].counter34.context);
}

static void _timer1_isr12(void)
{
	if (_timer_ack(1, TIMER_HALF_12, NULL))
		g_config[1].counter12.on_period(g_config[1].counter12.context);
}

static void _timer1_isr34(void)
{
	if (_timer_ack(1, TIMER_HALF_34, NULL))
		g_config[1].counter34.on_period(g_config[1].counter34.context);
}

static void _timer2_isr(void)
{
	u32 pending = _timer_ack(2, TIMER_HALF_BOTH, NULL);
	if ((pending & INTCTLSTAT_PRDINTSTAT12_MASK) && g_config[2].counter12.on_period)
		g_config[2].counter12.on_period(g_config[2].counter12.context);
	if ((pending & INTCTLSTAT_PRDINTSTAT34_MASK) && g_config[2].counter34.on_period)
		g_config[2].counter34.on_period(g_config[2].counter34.context);
}

static void _timer3_isr(void)
{
	u32 pending = _timer_ack(3, TIMER_HALF_BOTH, NULL);
	if ((pending & INTCTLSTAT_PRDINTSTAT12_MASK) && g_config[3].counter12.on_period)
		g_config[3].counter12.on_period(g_config[3].counter12.context);
	if ((pending & INTCTLSTAT_PRDINTSTAT34_MASK) && g_config[3].counter34.on_period)
		g_config[3].counter34.on_period(g_config[3].counter34.context);
}

static void _timer0_capture_isr(void)
{
	_timer_capture_irq(0, TIMER_HALF_12);
}

static void _timer1_capture_isr(void)
{
	_timer_capture_irq(1, TIMER_HALF_12);
}

static void _timer2_capture_isr(void)
{
	_timer_capture_irq(2, TIMER_HALF_BOTH);
}

static void _timer3_capture_isr(void)
{
	_timer_capture_irq(3, TIMER_HALF_BOTH);
}
