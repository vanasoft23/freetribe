#pragma once

#include "ft.h"
#include <am18x_map.h>

#define CHANNEL_X_MAX                   (0x1FUL)
#define CHANNEL(x)                      ((x) & CHANNEL_X_MAX)
#define QCHANNEL_X_MAX                  (0x7UL)
#define QCHANNEL(x)                     ((x) & QCHANNEL_X_MAX)
#define TR_WORD(addr)                   (((addr) & 0x1FUL) >> 2)

typedef enum {
	REGION_0,
	REGION_1,
	REGION_GLOBAL,
} edma_region_t;

typedef enum {
	EDMA_Q0,
	EDMA_Q1,                        // EDMA1 only
} emda_queue_t;

typedef enum {
	DMA_EVENT_TRIGGERED,
	DMA_MANUALLY_TRIGGERED,
	// DMA_CHAIN_TRIGGERED,
	QDMA_AUTO_TRIGGERED,
	// QDMA_LINK_TRIGGERED,
} edma_trigger_t;

typedef enum {
	// each sync event initiates the transfer of
	// one array (ACNT bytes) only
	FLAG_SYNCTYPE_A = 0,
	// each sync event initiates the transfer of
	// one frame (BCNT arrays of ACNT bytes)
	FLAG_SYNCTYPE_AB = BIT(0),
	FLAG_TRANS_EVT = BIT(1),
	FLAG_TRANS_INTR = BIT(2),
	FLAG_LAST_PAENTRY = BIT(3),
	FLAG_TCC_NORMAL = 0,
	FLAG_TCC_EARLY = BIT(4),
} conf_flags_t;

typedef struct {
	u32             src;            // source start address
	u32             dst;            // destination start address

	u16             a_cnt;          // (one array has a_cnt bytes)
	u16             b_cnt;          // (one frame has b_cnt arrays)

	u16             c_cnt;          // (one pa entry transfer has c_cnt frames)
#define LINK_NULL                       0xFFFFUL
#define LINK_NEXT(idx)                  (0x4000UL + ((idx) << 5))
	u16             link;           // linked as pa[n].link = &param[pa[n+1].index]

	u16             src_b_idx;      // distance (arrrys[n+1], array[n]) in a frame
	u16             dst_b_idx;      //

	u16             src_c_idx;      // A-sync  for distance (frames[n + 1].0, frames[n].last)
	u16             dst_c_idx;      // AB-sync for distance (frames[n + 1].0, frames[n].0)

	u8              priv_id;        // 0 --- EDMA3 master's privilege identification value
	u8              tcc;            // chain channel or interrupt bit index #
	u8              index;          // index of PaRAM set
	u8              reserved0[1];   // QCHANNEL --- customized by user
	                                // CHANNEL  --- first one identical with edma_conf_t.channel
	                                //              other ones can be chained as pa[n].tcc = pa[n + 1].index
	u32             flags;
} pa_conf_t;

typedef struct {
	pa_conf_t*      pa_conf;        // param sets as chained or linked
	u16             pa_cnt;         // elements count of pa_conf
	u8              tr_word;        // QCHANNEL only
	u8              reserved0[1];

	u8              channel;        // CHANNEL(n) or QCHANNEL(n)
	u8              region;         // edma_region_t
	u8              queue;          // emda_queue_t
	u8              trigger;        // edma_trigger_t
} edma_conf_t;

typedef struct {
	char*           status[8];
	u32             comp_actv;
	u16             queue_evts[2];
} edma_stat_t;

void per_edma_init(EDMA_con_t* econ, const edma_conf_t* conf);
void per_edma_param(EDMA_con_t* econ, const edma_conf_t* conf);
void per_edma_interrupt(EDMA_con_t* econ, const edma_conf_t* conf);
void per_edma_transfer(EDMA_con_t* econ, const edma_conf_t* conf);
bool per_edma_completed(EDMA_con_t* econ, const edma_conf_t* conf);
bool per_edma_status(const EDMA_con_t* econ, edma_stat_t* stat);
