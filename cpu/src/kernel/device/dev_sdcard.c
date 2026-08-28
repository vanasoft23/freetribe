/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

                                License

                   GNU AFFERO GENERAL PUBLIC LICENSE
                      Version 3, 19 November 2007

                           AGPL-3.0-or-later

 Freetribe is free software: you can redistribute it and/or modify it
under the terms of the GNU Affero General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
                  (at your option) any later version.

     Freetribe is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty
        of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
          See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    dev_sdcard.c.
 *
 * @brief   Configuration and handling of MMC/SD controller peripheral.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
			Original driver ported from am18x-lib by turmary
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "per_gpio.h"
#include "per_mmcsd.h"
#include "per_mmcsd_prot.h"

#include "dev_sdcard.h"

/*----- Macros -------------------------------------------------------*/

#ifdef DEBUG_SDDRIVER
#   define DLOG_SD(fmt, ...)  DLOG(fmt, ##__VA_ARGS__)
#else
#   define DLOG_SD(...)
#endif

#define MMCSDCON           MMCSD0

#define MMCSD_CLK          10,0,2
#define MMCSD_CMD          10,4,2
#define MMCSD_DAT0         10,8,2
#define MMCSD_DAT1        10,12,2
#define MMCSD_DAT2        10,16,2
#define MMCSD_DAT3        10,20,2
#define MMCSD_WP          10,24,2
#define MMCSD_INS         10,28,2

#define BUS_POWER_VOLTAGE    3300 //mV

#define LOW_CLK            400000 //Hz

#define MMC_RCA            0x1234

#define SDMMC_REG_RETRY    100000
#define SDMMC_CMD_RETRY        20
#define SDMMC_ARG_NULL          0
#define SDMMC_FIFO_CHUNK_WORDS (32u / sizeof(u32))
#define SDMMC_READ_STATE_RETRY  5000000u
#define SDMMC_WRITE_STATE_RETRY 5000000u
#define SDMMC_BUSY_STATE_RETRY  5000000u

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	u16             rca;
	u8              is_mmc:1;
	u8              is_hc:1;
	u8              is_bus4bit:1;
	sdp_cur_stat_t  ci_stat;
	sdp_r1_stat_t   r1_stat;
	CID_t           cid;
	CSD_t           csd;
} sd_sm_t;

typedef struct {
	int rt;
	const char* estr;
} sdmmc_estr_t;

/*----- Static variable definitions ----------------------------------*/

sd_sm_t sd_sm[1];

#define RT_ESTR_PAIR(X)    { X, #X }

sdmmc_estr_t sdmmc_estr[] = {
	RT_ESTR_PAIR(SDCARD_OK),
	RT_ESTR_PAIR(SDCARD_NO_RESPONSE),
	RT_ESTR_PAIR(SDCARD_CRC_ERROR),
	RT_ESTR_PAIR(SDCARD_UNSUPPORTED),
	RT_ESTR_PAIR(SDCARD_ERROR),
};

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static int                    _sdmmc_inf_init(void);
static inline u32             _sdmmc_resp(void);
static t_sdcard_status        _sdmmc_print_r1(void);
static t_sdcard_status        _sdmmc_get_cid(void);
static u32                    _sdmmc_cmd_stat(int nr);
static t_sdcard_status        _sdmmc_cmd(int nr, u32 arg);
static t_sdcard_status        _sdmmc_acmd(int nr, u32 arg);
static inline t_sdcard_status _sdmmc_cmd_noarg(int nr);
static t_mmcsd_dat_state      _sdmmc_rd_done_state(void);
static t_sdcard_status        _sdmmc_read_fail(
	const char       *tag,
	u32               blk_nr,
	u32               blk_cnt,
	u32               words_done,
	u32               total_words,
	t_mmcsd_dat_state ds,
	t_sdcard_status   status
);
static t_sdcard_status        _ACMD41(void);
static t_sdcard_status        _CMD1(void);
static void                   _sdmmc_log_read_state(
	const char       *tag,
	u32               blk_nr,
	u32               blk_cnt,
	u32               words_done,
	u32               total_words,
	t_mmcsd_dat_state ds
);
static void                   _sdmmc_log_write_state(
	const char       *tag,
	u32               blk_nr,
	u32               blk_cnt,
	u32               words_done,
	u32               total_words,
	t_mmcsd_dat_state ds
);

// Protocol
static t_sdcard_status        _sdmmc_card_init(void);
static t_sdcard_status        _sdmmc_get_csd(void);
static t_sdcard_status        _sdmmc_speed_up(void);
static t_sdcard_status        _sdmmc_get_classes(void);
static t_sdcard_status        _sdmmc_sel_card(bool sel);
static t_sdcard_status        _sdmmc_set_blocklen(void);
static t_sdcard_status        _sdmmc_max_buswidth(void);

/*----- Extern function implementations ------------------------------*/

const char* dev_sdcard_err_string(int rt) {
	return sdmmc_estr[-rt].estr;
}

t_sdcard_status dev_sdcard_read(u32 blk_nr, u32 blk_cnt, u32* buf) {
	t_mmcsd_dat_state ds;
	t_mmcsd_misc misc;
	t_sdcard_status r;
	u32 i;
	u32 ii;
	u32 wait_count;
	u32 total_words;

	if (!buf || blk_cnt == 0) {
		return SDCARD_ERROR;
	}

	total_words = (blk_cnt * MMCSD_BLOCK_SIZE) / sizeof(u32);

	misc.blkcnt = blk_cnt;
	misc.mflags = MMCSD_MISC_F_READ | MMCSD_MISC_F_FIFO_RST | MMCSD_MISC_F_FIFO_32B;
	mmcsd_cntl_misc(MMCSDCON, &misc);

	int cmd_nr = CMD17R1_READ_SINGLE_BLOCK;
	if (blk_cnt > 1) cmd_nr = CMD18R1_READ_MULTIPLE_BLOCK;

	u32 arg = (sd_sm->is_hc) ? (blk_nr) : (blk_nr << MASK_OFFSET(MMCSD_BLOCK_SIZE));
	r = _sdmmc_cmd(cmd_nr, arg);
	if (r != SDCARD_OK) {
		return _sdmmc_read_fail(
			"cmd",
			blk_nr,
			blk_cnt,
			0,
			total_words,
			MMCSD_SD_NONE,
			r
		);
	}

	// // dirty timing-dependancy fix
	// #define SDMMC_READ_SETTLE_US 1000
	// delay_block_us(SDMMC_READ_SETTLE_US);

	i = 0;
	wait_count = 0;
	while (i < total_words) {
		if ((ds = mmcsd_rd_state(MMCSDCON)) == MMCSD_SD_OK) {
			return _sdmmc_read_fail(
				"early-datdne",
				blk_nr,
				blk_cnt,
				i,
				total_words,
				ds,
				SDCARD_ERROR
			);
		}
		if (ds == MMCSD_SD_TOUT || wait_count++ > SDMMC_READ_STATE_RETRY) {
			return _sdmmc_read_fail(
				"feed-timeout",
				blk_nr,
				blk_cnt,
				i,
				total_words,
				ds,
				SDCARD_NO_RESPONSE
			);
		}
		if (ds == MMCSD_SD_CRC_ERR) {
			return _sdmmc_read_fail(
				"crc",
				blk_nr,
				blk_cnt,
				i,
				total_words,
				ds,
				SDCARD_CRC_ERROR
			);
		}
		if (ds == MMCSD_SD_RECVED) {
			wait_count = 0;
			for (ii = 0; (ii < SDMMC_FIFO_CHUNK_WORDS) && (i < total_words); ii++) {
				buf[i++] = mmcsd_read(MMCSDCON);
			}
		}
		// DLOG_SD("   *** ST1=0x%.8X ***\n", MMCSDCON->MMCST1);
	}

	wait_count = 0;
	while ((ds = _sdmmc_rd_done_state()) != MMCSD_SD_OK) {
		if (ds == MMCSD_SD_TOUT || wait_count++ > SDMMC_READ_STATE_RETRY) {
			return _sdmmc_read_fail(
				"done-timeout",
				blk_nr,
				blk_cnt,
				i,
				total_words,
				ds,
				SDCARD_NO_RESPONSE
			);
		}
		if (ds == MMCSD_SD_CRC_ERR) {
			return _sdmmc_read_fail(
				"done-crc",
				blk_nr,
				blk_cnt,
				i,
				total_words,
				ds,
				SDCARD_CRC_ERROR
			);
		}
	}

	DLOG_SD("SDMMC READ %u bytes\n", (unsigned)(i * sizeof(u32)));

	if (blk_cnt > 1) {
		r = _sdmmc_cmd_noarg(CMD12R1b_STOP_TRANSMISSION);
		if (r != SDCARD_OK) {
			return _sdmmc_read_fail(
				"stop",
				blk_nr,
				blk_cnt,
				i,
				total_words,
				ds,
				r
			);
		}

		wait_count = 0;
		do {
			ds = mmcsd_busy_state(MMCSDCON);
			if (ds == MMCSD_SD_TOUT || wait_count++ > SDMMC_BUSY_STATE_RETRY) {
				return _sdmmc_read_fail(
					"stop-busy-timeout",
					blk_nr,
					blk_cnt,
					i,
					total_words,
					ds,
					SDCARD_NO_RESPONSE
				);
			}
			if (ds == MMCSD_SD_CRC_ERR) {
				return _sdmmc_read_fail(
					"stop-busy-crc",
					blk_nr,
					blk_cnt,
					i,
					total_words,
					ds,
					SDCARD_CRC_ERROR
				);
			}
		} while (ds == MMCSD_SD_BUSY);
	} else {
		sd_sm->ci_stat = SDP_TRAN;
	}

	return SDCARD_OK;
}

t_sdcard_status dev_sdcard_write(u32 blk_nr, u32 blk_cnt, const u32* buf) {
	t_mmcsd_dat_state ds;
	t_mmcsd_misc misc;
	t_sdcard_status r;
	u32 i;
	u32 ii;
	u32 wait_count;
	u32 total_words;
	bool feed_initial_fifo;

	if (!buf || blk_cnt == 0) {
		return SDCARD_ERROR;
	}

	total_words = (blk_cnt * MMCSD_BLOCK_SIZE) / sizeof(u32);

	misc.blkcnt = blk_cnt;
	misc.mflags = MMCSD_MISC_F_WRITE | MMCSD_MISC_F_FIFO_RST | MMCSD_MISC_F_FIFO_32B;
	mmcsd_cntl_misc(MMCSDCON, &misc);

	int cmd_nr = CMD24R1_WRITE_BLOCK;
	if (blk_cnt > 1) cmd_nr = CMD25R1_WRITE_MULTIPLE_BLOCK;

	for (i = 0; (i < SDMMC_FIFO_CHUNK_WORDS) && (i < total_words);) {
		mmcsd_write(MMCSDCON, buf[i++]);
	}

	u32 arg = (sd_sm->is_hc) ? (blk_nr) : (blk_nr << MASK_OFFSET(MMCSD_BLOCK_SIZE));
	r = _sdmmc_cmd(cmd_nr, arg);
	if (r != SDCARD_OK) return r;

	mmcsd_trigger_data_transfer(MMCSDCON);

	feed_initial_fifo = true;
	wait_count = 0;
	while (i < total_words) {
		if ((ds = mmcsd_wr_state(MMCSDCON)) == MMCSD_SD_OK) {
			_sdmmc_log_write_state("early-datdne", blk_nr, blk_cnt, i, total_words, ds);
			return SDCARD_ERROR;
		}
		if (ds == MMCSD_SD_TOUT || wait_count++ > SDMMC_WRITE_STATE_RETRY) {
			_sdmmc_log_write_state("feed-timeout", blk_nr, blk_cnt, i, total_words, ds);
			return SDCARD_NO_RESPONSE;
		}
		if (ds == MMCSD_SD_CRC_ERR) {
			return SDCARD_CRC_ERROR;
		}
		// DLOG_SD("   *** ST1=0x%.8X ***\n", MMCSDCON->MMCST1);
		if (feed_initial_fifo || ds == MMCSD_SD_SENT) {
			feed_initial_fifo = false;
			wait_count = 0;
			for (ii = 0; (ii < SDMMC_FIFO_CHUNK_WORDS) && (i < total_words); ii++) {
				mmcsd_write(MMCSDCON, buf[i++]);
			}
		}
	}

	wait_count = 0;
	while ((ds = mmcsd_wr_state(MMCSDCON)) != MMCSD_SD_OK) {
		if (ds == MMCSD_SD_TOUT || wait_count++ > SDMMC_WRITE_STATE_RETRY) {
			_sdmmc_log_write_state("done-timeout", blk_nr, blk_cnt, i, total_words, ds);
			return SDCARD_NO_RESPONSE;
		}
		if (ds == MMCSD_SD_CRC_ERR) {
			return SDCARD_CRC_ERROR;
		}
	}

	DLOG_SD("SDMMC WRITE %u bytes\n", (unsigned)(i * sizeof(u32)));

	if (blk_cnt <= 1) {
		goto done;
	}

	r = _sdmmc_cmd_noarg(CMD12R1b_STOP_TRANSMISSION);
	if (r != SDCARD_OK) return r;

	wait_count = 0;
	do {
		ds = mmcsd_busy_state(MMCSDCON);
		if (ds == MMCSD_SD_TOUT || wait_count++ > SDMMC_BUSY_STATE_RETRY) {
			_sdmmc_log_write_state("stop-busy-timeout", blk_nr, blk_cnt, i, total_words, ds);
			return SDCARD_NO_RESPONSE;
		}
		if (ds == MMCSD_SD_CRC_ERR) {
			return SDCARD_CRC_ERROR;
		}
		//DLOG_SD("   *** ST0=0x%.8X RSP=0x%.8X ***\n", MMCSDCON->MMCST0, MMCSDCON->MMCRSP[3]);
	} while (ds == MMCSD_SD_BUSY);

done:
	sd_sm->ci_stat = SDP_TRAN;

	return SDCARD_OK;
}

t_sdcard_status dev_sdcard_init(void) {

	t_sdcard_status r;

	if (!dev_sdcard_present()) {
		return SDCARD_NO_RESPONSE;
	}

	_sdmmc_inf_init();

	r = _sdmmc_card_init();
	if (r != SDCARD_OK) {
		DLOG_SD("SDMMC card init %s\n", dev_sdcard_err_string(r));
		return r;
	}

	r = _sdmmc_get_csd();
	if (r != SDCARD_OK) {
		DLOG_SD("SDMMC get csd %s\n", dev_sdcard_err_string(r));
		return r;
	}

	r = _sdmmc_speed_up();
	r = _sdmmc_get_classes();

	r = _sdmmc_sel_card(AM18X_TRUE);
	if (r != SDCARD_OK) {
		DLOG_SD("SDMMC sel card %s\n", dev_sdcard_err_string(r));
		return r;
	}

	r = _sdmmc_set_blocklen();
	if (r != SDCARD_OK) {
		DLOG_SD("SDMMC set block length %s\n", dev_sdcard_err_string(r));
		return r;
	}

	r = _sdmmc_max_buswidth();
	DLOG_SD("SDMMC bus width = %dBIT %s\n", (sd_sm->is_bus4bit? 4: 1), dev_sdcard_err_string(r));

	return r;
}

bool dev_sdcard_present(void) {

	bool inserted = !per_gpio_get_indexed(PIN_EMA_A_16__MMCSD0_DAT5);
	return inserted;
}

u32 dev_sdcard_get_sector_count(void) {

	return sdprot_sector_count(&sd_sm->csd);
}

t_sdcard_status dev_sdcard_terminate(void) {

	t_sdcard_status r = SDCARD_OK;
	u32 reg, msk, v;

	// // Stop any in-flight transfers
	// r = _sdmmc_cmd_noarg(CMD12R1b_STOP_TRANSMISSION);
	// if (r != SDCARD_OK) return r; // SDCARD_NO_RESPONSE

	// Put card back to idle
	_sdmmc_cmd_noarg(CMD0_GO_IDLE_STATE);
	
	// Drain/reset the FIFO
	MMCSDCON->MMCFIFOCTL = 0;

	// Clear status registers
	MMCSDCON->MMCRSP[0] = 0;
	MMCSDCON->MMCRSP[1] = 0;
	MMCSDCON->MMCRSP[2] = 0;
	MMCSDCON->MMCRSP[3] = 0;
	MMCSDCON->MMCIM = 0x0UL;

	// Disable the clock
	reg = MMCSDCON->MMCCLK;
	MMCSDCON->MMCCLK = FIELD_SET(reg, MMCCLK_CLKEN_MASK, MMCCLK_CLKEN_low);

	// Put MMC/SD controller in its reset state
	msk = MMCCTL_CMDRST_MASK | MMCCTL_DATRST_MASK;
	v = MMCCTL_CMDRST_disabled | MMCCTL_DATRST_disabled;
	MMCSDCON->MMCCTL = FIELD_SET(0, msk, v);

	return r;

}



/*----- Static function implementations ------------------------------*/

static int _sdmmc_inf_init(void) {
	mmcsd_conf_t conf[1];
	u32 freq;

	conf->freq = LOW_CLK;
	conf->timeout_rsp = TIMEOUT_RSP_MAX;
	conf->timeout_dat = TIMEOUT_DAT_MAX;
	mmcsd_con_init(MMCSDCON, conf);

	mmcsd_set_freq(MMCSDCON, LOW_CLK);

	return 0;
}

static inline u32 _sdmmc_resp(void) {
	t_mmcsd_resp resp;

	mmcsd_get_resp(MMCSDCON, MMCSD_RESP_SHORT, &resp);
	return resp.v[0];
}

static t_sdcard_status _sdmmc_print_r1(void) {
	union {
		u32 i;
		sdp_r1_stat_t r1_stat;
	}u;

	u.i = _sdmmc_resp();
	sd_sm->r1_stat = u.r1_stat;

	sdprot_print_r1_stat(&sd_sm->r1_stat);

	return SDCARD_OK;
}

static t_sdcard_status _sdmmc_get_cid(void) {
	t_mmcsd_resp resp;

	mmcsd_get_resp(MMCSDCON, MMCSD_RESP_LONG, &resp);

	sdprot_get_cid(&sd_sm->cid, resp.v);

	return SDCARD_OK;
}

static u32 _sdmmc_cmd_stat(int nr) {
	u32 stat;

	if (sdprot_resp_crc(nr) == 0) {
		stat = mmcsd_cmd_state(MMCSDCON, AM18X_FALSE);
	} else {
		stat = mmcsd_cmd_state(MMCSDCON, AM18X_TRUE);
	}
	return stat;
}

static t_sdcard_status _sdmmc_cmd(int nr, u32 arg) {
	sdcard_response_t srt;
	t_mmcsd_cmd cmd;
	u32 stat;
	t_sdcard_status r;
	int i;

	r = SDCARD_OK;
	cmd.index = nr;
	cmd.arg = arg;

	if (sdprot_next_stat(nr, sd_sm->ci_stat) == SDP_INV) {
		DLOG_SD("SDPROT\tCurrent State %s with CMD%d\n", 
			sdprot_stat_name(sd_sm->ci_stat), nr);
		// r = SDCARD_UNSUPPORTED;
		// goto done;
	}

	cmd.cflags = 0;
	if (sd_sm->ci_stat >= SDP_STBY && sd_sm->ci_stat < SDP_CNT) {
		cmd.cflags |= MMCSD_CMD_F_PPLEN;
	}
	switch(srt = sdprot_resp_type(nr)) {
	case SDCARD_48BIT_RSP:
		cmd.cflags |= MMCSD_CMD_F_RSP | MMCSD_CMD_F_SHORT;
		if (sdprot_resp_crc(nr)) {
			cmd.cflags |= MMCSD_CMD_F_CRC;
		}
		break;
	case SDCARD_136BIT_RSP:
		cmd.cflags |= MMCSD_CMD_F_RSP | MMCSD_CMD_F_LONG;
		break;
	case SDCARD_NONE_RSP:
	default:
		cmd.cflags |= MMCSD_CMD_F_NORSP;
		break;
	}
	if (sdprot_need_data(nr) != SDPROT_NO_DATA) {
		cmd.cflags |= MMCSD_CMD_F_DATA;
		if (sdprot_need_data(nr) == SDPROT_READ_DATA) {
			cmd.cflags |= MMCSD_CMD_F_READ;
		} else {
			cmd.cflags |= MMCSD_CMD_F_WRITE;
		}
	}

	if (sdprot_need_busy(nr)) {
		cmd.cflags |= MMCSD_CMD_F_BUSY;
	}

	mmcsd_send_cmd(MMCSDCON, &cmd);

	if (sdprot_need_data(nr) == SDPROT_WRITE_DATA) {
		// Do not poll command status here: write-data commands can raise DXRDY
		// before software observes RSPDNE, and the data loop owns that status.
		stat = sdprot_next_stat(nr, sd_sm->ci_stat);
		if (stat != sd_sm->ci_stat) {
			sd_sm->ci_stat = stat;
		}
		goto done;
	}

	for(i = 0;;) {
		stat = _sdmmc_cmd_stat(nr);
		if (stat == MMCSD_SC_RSP_TOUT || stat == MMCSD_SC_RSP_OK || stat == MMCSD_SC_CRC_ERR) {
			break;
		}
		if (i++ > SDMMC_REG_RETRY) {
			DLOG_SD("%s() *** error stat = %d ***\n", __func__, stat);
			r = SDCARD_NO_RESPONSE;
			goto done;
		}
	}
	if (stat != MMCSD_SC_RSP_OK) {
		DLOG_SD("*** MMCCMD=0x%.8X ARG=0x%.8X ***", MMCSDCON->MMCCMD, MMCSDCON->MMCARGHL);
		DLOG_SD("   *** ST0=0x%.8X RSP=0x%.8X ***\n", MMCSDCON->MMCST0, MMCSDCON->MMCRSP[3]);
	}
	if (stat == MMCSD_SC_CRC_ERR) {
		r = SDCARD_CRC_ERROR;
		goto done;
	}
	if (stat == MMCSD_SC_RSP_TOUT) {
		r = SDCARD_NO_RESPONSE;
		goto done;
	}
	DLOG_SD("SDPROT\tCMD%d OK\n", nr);

	stat = sdprot_next_stat(nr, sd_sm->ci_stat);
	if (stat != sd_sm->ci_stat) {
		DLOG_SD("SDPROT\tTransition from %s to %s\n", 
			sdprot_stat_name(sd_sm->ci_stat),
			sdprot_stat_name(stat));
		sd_sm->ci_stat = stat;
	}

done:
	return r;
}

static t_sdcard_status _sdmmc_acmd(int nr, u32 arg) {
	t_sdcard_status r;

	r = _sdmmc_cmd(CMD55R1_APP_CMD, sd_sm->rca << 16);
	if (r != SDCARD_OK) {
		DLOG_SD("SDMMC status = 0x%.8X\n", _sdmmc_resp());
		return r;
	}

	//_sdmmc_print_r1();

	return _sdmmc_cmd(nr, arg);
}

static inline t_sdcard_status _sdmmc_cmd_noarg(int nr) {
	return _sdmmc_cmd(nr, SDMMC_ARG_NULL);
}

static t_mmcsd_dat_state _sdmmc_rd_done_state(void) {
	u32 reg;

	reg = MMCSDCON->MMCST0;
	if (FIELD_GET(reg, MMCST0_CRCRD_MASK) == MMCST0_CRCRD_detected) {
		return MMCSD_SD_CRC_ERR;
	}
	if (FIELD_GET(reg, MMCST0_TOUTRD_MASK) == MMCST0_TOUTRD_occurred) {
		return MMCSD_SD_TOUT;
	}
	if (FIELD_GET(reg, MMCST0_DATDNE_MASK) == MMCST0_DATDNE_done) {
		return MMCSD_SD_OK;
	}

	return MMCSD_SD_NONE;
}

static t_sdcard_status _sdmmc_read_fail(
	const char* tag,
	u32 blk_nr,
	u32 blk_cnt,
	u32 words_done,
	u32 total_words,
	t_mmcsd_dat_state ds,
	t_sdcard_status status
) {
	t_sdcard_status stop_status;
	u32 wait_count;

	_sdmmc_log_read_state(tag, blk_nr, blk_cnt, words_done, total_words, ds);

	if (sd_sm->ci_stat == SDP_DATA) {
		stop_status = _sdmmc_cmd_noarg(CMD12R1b_STOP_TRANSMISSION);
		if (stop_status == SDCARD_OK) {
			wait_count = 0;
			do {
				ds = mmcsd_busy_state(MMCSDCON);
				if (ds == MMCSD_SD_TOUT || wait_count++ > SDMMC_BUSY_STATE_RETRY) {
					DLOG(
						"sd read abort busy failed: tag=%s ds=%d st0=0x%.8X st1=0x%.8X",
						tag,
						(int)ds,
						(unsigned)MMCSDCON->MMCST0,
						(unsigned)MMCSDCON->MMCST1
					);
					break;
				}
			} while (ds == MMCSD_SD_BUSY);
		} else {
			DLOG(
				"sd read abort stop failed: tag=%s res=%d state=%u st0=0x%.8X st1=0x%.8X",
				tag,
				(int)stop_status,
				(unsigned)sd_sm->ci_stat,
				(unsigned)MMCSDCON->MMCST0,
				(unsigned)MMCSDCON->MMCST1
			);
		}
	}

	MMCSDCON->MMCFIFOCTL = 0;
	return status;
}

static void _sdmmc_log_read_state(
	const char* tag,
	u32 blk_nr,
	u32 blk_cnt,
	u32 words_done,
	u32 total_words,
	t_mmcsd_dat_state ds
) {
	DLOG(
		"sd read %s: ds=%d words=%u/%u blk=%u cnt=%u "
		"st0=0x%.8X st1=0x%.8X cmd=0x%.8X arg=0x%.8X cidx=0x%.8X "
		"im=0x%.8X fifo=0x%.8X nblk=%u nblc=%u blen=%u rsp=0x%.8X state=%u hc=%u",
		tag,
		(int)ds,
		(unsigned)words_done,
		(unsigned)total_words,
		(unsigned)blk_nr,
		(unsigned)blk_cnt,
		(unsigned)MMCSDCON->MMCST0,
		(unsigned)MMCSDCON->MMCST1,
		(unsigned)MMCSDCON->MMCCMD,
		(unsigned)MMCSDCON->MMCARGHL,
		(unsigned)MMCSDCON->MMCCIDX,
		(unsigned)MMCSDCON->MMCIM,
		(unsigned)MMCSDCON->MMCFIFOCTL,
		(unsigned)MMCSDCON->MMCNBLK,
		(unsigned)MMCSDCON->MMCNBLC,
		(unsigned)MMCSDCON->MMCBLEN,
		(unsigned)MMCSDCON->MMCRSP[3],
		(unsigned)sd_sm->ci_stat,
		(unsigned)sd_sm->is_hc
	);
}

static void _sdmmc_log_write_state(
	const char* tag,
	u32 blk_nr,
	u32 blk_cnt,
	u32 words_done,
	u32 total_words,
	t_mmcsd_dat_state ds
) {
	DLOG(
		"sd write %s: ds=%d words=%u/%u blk=%u cnt=%u "
		"st0=0x%.8X st1=0x%.8X cmd=0x%.8X arg=0x%.8X cidx=0x%.8X "
		"im=0x%.8X fifo=0x%.8X nblk=%u nblc=%u blen=%u rsp=0x%.8X state=%u hc=%u",
		tag,
		(int)ds,
		(unsigned)words_done,
		(unsigned)total_words,
		(unsigned)blk_nr,
		(unsigned)blk_cnt,
		(unsigned)MMCSDCON->MMCST0,
		(unsigned)MMCSDCON->MMCST1,
		(unsigned)MMCSDCON->MMCCMD,
		(unsigned)MMCSDCON->MMCARGHL,
		(unsigned)MMCSDCON->MMCCIDX,
		(unsigned)MMCSDCON->MMCIM,
		(unsigned)MMCSDCON->MMCFIFOCTL,
		(unsigned)MMCSDCON->MMCNBLK,
		(unsigned)MMCSDCON->MMCNBLC,
		(unsigned)MMCSDCON->MMCBLEN,
		(unsigned)MMCSDCON->MMCRSP[3],
		(unsigned)sd_sm->ci_stat,
		(unsigned)sd_sm->is_hc
	);
}

static t_sdcard_status _ACMD41(void) {
	t_sdcard_status r;
	u32 ocr;

	ocr = OCR_VOLTAGE_WINDOW(BUS_POWER_VOLTAGE);
	do {
		// 30 HCS(OCR[30])
		// 23:0 Vdd Voltage Window(OCR[23:0])
		r = _sdmmc_acmd(ACMD41R3_SD_SEND_OP_COND, (ocr  | OCR_CCS) & ~OCR_PowerUpEnd);
		if (r != SDCARD_OK) {
			DLOG_SD("SDPROT\tNot SD Memory Card\n");
			return SDCARD_UNSUPPORTED;
		}
		ocr = _sdmmc_resp();
		if (0 == (ocr & OCR_VOLTAGE_WINDOW(BUS_POWER_VOLTAGE))) {
			return SDCARD_UNSUPPORTED;
		}
		if (ocr & OCR_PowerUpEnd) break;

		DLOG_SD("SDPROT\tcard returns busy or host omitted voltage range\n");
		DLOG_SD("%s() ocr = 0x%.8X\n", __func__, ocr);

		sd_sm->ci_stat = SDP_IDLE;
	} while (AM18X_TRUE);

	DLOG_SD("%s() ocr = 0x%.8X\n", __func__, ocr);

	if (ocr & OCR_CCS) {
		sd_sm->is_hc = AM18X_TRUE;
	} else {
		sd_sm->is_hc = AM18X_FALSE;
	}

	return SDCARD_OK;
}

static t_sdcard_status _CMD1(void) {
	t_sdcard_status r;
	u32 ocr, msk;

	ocr = OCR_VOLTAGE_WINDOW(BUS_POWER_VOLTAGE);
	msk = OCR_PowerUpEnd;
	do {
		r = _sdmmc_cmd(CMD1R3_SEND_OP_COND, ocr & ~OCR_PowerUpEnd);
		if (r != SDCARD_OK) {
			DLOG_SD("MMCPROT\tcards with non compatible voltage range\n");
			return r;
		}
		ocr = _sdmmc_resp();
		if ((ocr & msk) == msk) break;

		DLOG_SD("MMCPROT\tcard is busy or\n");
		DLOG_SD("\thost omitted voltage range\n");
		sd_sm->ci_stat = SDP_IDLE;
	} while (AM18X_TRUE);

	if (ocr & MOCR_VOLTAGE_165to195) {
		DLOG_SD("MMCPROT\tLow Voltage MultiMediaCard\n");
	} else {
		DLOG_SD("MMCPROT\tHigh Voltage MultiMediaCard\n");
	}

	DLOG_SD("%s() ocr = 0x%.8X\n", __func__, ocr);

	if (0 == (ocr & OCR_VOLTAGE_WINDOW(BUS_POWER_VOLTAGE))) {
		return SDCARD_UNSUPPORTED;
	}
	return SDCARD_OK;
}

// Protocol
// 4.2.3 Card Initialization and Identification Process
static t_sdcard_status _sdmmc_card_init(void) {
	t_sdcard_status r;
	int i;

	sd_sm->rca = 0;
	sd_sm->is_mmc = 0;
	sd_sm->is_bus4bit = 0;
	sd_sm->ci_stat = SDP_IDLE;

	for (i = 0; i < 1000; i++);

	_sdmmc_cmd_noarg(CMD0_GO_IDLE_STATE);
	DLOG_SD("SDPROT\tIdle State(idle)\n");

	r = _sdmmc_cmd(CMD8R7_SEND_IF_COND, CMD8_VHS_27to36 | CMD8_CHECK_PATTERN);
	DLOG_SD("SDMMC cmd8() %s\n", dev_sdcard_err_string(r));

	if (r == SDCARD_NO_RESPONSE) {
		DLOG_SD("SDPROT\tVer2.00 or later SD Memory Card(voltage mismatch)\n");
		DLOG_SD("\tor Ver1.X SD Memory Card\n");
		DLOG_SD("\tor not SD Memory Card\n");
	} else if ((_sdmmc_resp() & CMD8_CHECK_PATTERN_MASK) == CMD8_CHECK_PATTERN) {
		DLOG_SD("SDPROT\tVer2.00 or later SD Memory Card\n");
	} else {
		// unsupported
		return SDCARD_UNSUPPORTED;
	}

	if ((r = _ACMD41()) == SDCARD_OK) {
		DLOG_SD("SDPROT\tCard returns ready\n");
		DLOG_SD("\tVer1.X Standard Capacity SD Memory Card\n");
	} else {
		DLOG_SD("SDPROT\tNo Response(Non valid command)\n");
		DLOG_SD("\tMust be a MultiMediaCard\n");
		DLOG_SD("SDPROT\tStart MultiMediaCard initialization process\n");
		DLOG_SD("\tstarting at CMD1\n");

		if ((r = _CMD1()) != SDCARD_OK) {
			return r;
		}
		sd_sm->is_mmc = 1;
		sd_sm->rca = MMC_RCA;
	}

	DLOG_SD("SDPROT\tReady State(ready)\n");

	for (i = 0; i < SDMMC_CMD_RETRY; i++) {
		r = _sdmmc_cmd_noarg(CMD2R2_ALL_SEND_CID);
		if (r == SDCARD_OK) break;
	}
	if (r != SDCARD_OK) {
		return r;
	}
	DLOG_SD("SDPROT\tIdentification State(ident)\n");

	_sdmmc_get_cid();
	// sdprot_print_cid(&sd_sm->cid);

	for (i = 0; i < SDMMC_CMD_RETRY; i++) {
		u32 arg = 0;

		if (sd_sm->is_mmc) {
			arg = sd_sm->rca << 16;
		}
		r = _sdmmc_cmd(CMD3R6_SEND_RELATIVE, arg);
		if (r == SDCARD_OK) break;
	}
	if (r != SDCARD_OK) {
		return r;
	}

	DLOG_SD("SDPROT\tCard responds with new RCA\n");

	if (!sd_sm->is_mmc) {
		sd_sm->rca = (_sdmmc_resp() >> 16);
	}
	DLOG_SD("SDMMC new RCA = 0x%.4X\n", sd_sm->rca);

	DLOG_SD("SDPROT\tcard identification mode <-> data transfer mode\n");
	DLOG_SD("SDPROT\tStand by State(stby)\n");

	return r;
}

static t_sdcard_status _sdmmc_get_csd(void) {
	t_mmcsd_resp lngrsp;
	t_sdcard_status r;
	int i;

	for (i = 0; i < SDMMC_CMD_RETRY; i++) {
		r = _sdmmc_cmd(CMD9R2_SEND_CSD, sd_sm->rca << 16);
		if (r == SDCARD_OK) break;
	}
	if (r != SDCARD_OK) {
		return r;
	}

	mmcsd_get_resp(MMCSDCON, MMCSD_RESP_LONG, &lngrsp);
	sdprot_get_csd(&sd_sm->csd, lngrsp.v);
	// sdprot_print_csd(&sd_sm->csd);

	return r;
}

static t_sdcard_status _sdmmc_speed_up(void) {
	u32 speed;

	speed = sdprot_trans_speed(&sd_sm->csd);
	mmcsd_set_freq(MMCSDCON, speed);

	return SDCARD_OK;
}

static t_sdcard_status _sdmmc_get_classes(void) {
	u32 ccc;
	int i;

	ccc = sd_sm->csd.CCC;

	DLOG_SD("SDMMC Card Supported Classes:");
	for (i = 0; i < 12; i++) {
		if (ccc & BIT(i)) {
			DLOG_SD(" %d", i);
		}
	}
	DLOG_SD("\n");

	DLOG_SD("SDMMC Card Size %u bytes\n", sdprot_device_size(&sd_sm->csd));

	return SDCARD_OK;
}

static t_sdcard_status _sdmmc_sel_card(bool sel) {
	u32 arg;
	t_sdcard_status r;

	arg = sel? (sd_sm->rca << 16): 0;
	r = _sdmmc_cmd(CMD7R1b_SEL_UNSEL_CARD, arg);
	if (r != SDCARD_OK) {
		return r;
	}

	// _sdmmc_print_r1();

	return SDCARD_OK;
}

static t_sdcard_status _sdmmc_set_blocklen(void) {
	if (sd_sm->is_hc) {
		return SDCARD_OK;
	}

	if ((1u << sd_sm->csd.READ_BL_LEN) < MMCSD_BLOCK_SIZE
	||  (1u << sd_sm->csd.WRITE_BL_LEN) < MMCSD_BLOCK_SIZE) {
		return SDCARD_UNSUPPORTED;
	}

	return _sdmmc_cmd(CMD16R1_SET_BLOCKLEN, MMCSD_BLOCK_SIZE);
}

static t_sdcard_status _sdmmc_max_buswidth(void) {
	t_sdcard_status r;

	if (sd_sm->is_mmc) {
		return SDCARD_OK;
	}

	r = _sdmmc_acmd(ACMD6R1_SET_BUS_WIDTH, ACMD6_BW_4BIT);
	if (r == SDCARD_OK) {
		sd_sm->is_bus4bit = 1;
		sd_sm->ci_stat = SDP_TRAN;
	}

	if (sd_sm->is_bus4bit) {
		t_mmcsd_misc misc;

		misc.mflags = MMCSD_MISC_F_BUS4BIT;
		mmcsd_cntl_misc(MMCSDCON, &misc);        
	}
	return r;
}

/*----- End of file --------------------------------------------------*/
