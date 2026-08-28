/*----------------------------------------------------------------------
MIT License

Copyright (c) 2013 - turmary@126.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


Original work by turmary@126.com, modified by vanasoft23 for freetribe.
----------------------------------------------------------------------*/

/**
 * @file    per_mmcsd_prot.c.
 *
 * @brief   MMC/SD protocol specific code.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "am18x_lib.h"
#include "per_mmcsd_prot.h"

/*----- Macros -------------------------------------------------------*/

#ifdef DEBUG_SDDRIVER
#   define DLOG_SD(fmt, ...)  DLOG(fmt, ##__VA_ARGS__)
#else
#   define DLOG_SD(...)
#endif

#define IDLE    SDP_IDLE
#define REDY    SDP_READY
#define IDNT    SDP_IDENT
#define STBY    SDP_STBY
#define TRAN    SDP_TRAN
#define DATA    SDP_DATA
#define RCV     SDP_RCV
#define PRG     SDP_PRG
#define DIS     SDP_DIS
#define INA     SDP_INA
#define INV     SDP_INV

#define SDPROT_SECTOR_SIZE 512u

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	int         key;
	const char* val;
} kvp_t;

/*----- Static variable definitions ----------------------------------*/

// 4.8 Card State Transition Table
static u8 card_stat_trans_table[/*cmd index*/][SDP_CNT] = {
#define RESERVED_CMD    {INV,  INV,  INV,  INV,  INV,  INV,  INV,  INV,  INV,  }

	//IDLE,READY,INDENT,STBY,TRAN, DATA, RCV,  PRG,  DIS
	// class 0
	{IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, IDLE, }, // CMD0
	// The MultiMediaCard 4.8 Card State Transition Table
	{REDY, INV,  INV,  INV,  INV,  INV,  INV,  INV,  INV,  }, // CMD1
	{INV,  IDNT, INV,  INV,  INV,  INV,  INV,  INV,  INV,  }, // CMD2
	{INV,  INV,  STBY, STBY, INV,  INV,  INV,  INV,  INV,  }, // CMD3
	{INV,  INV,  INV,  STBY, INV,  INV,  INV,  INV,  INV,  }, // CMD4
	RESERVED_CMD,                         // CMD5
	{INV,  INV,  INV,  INV,  DATA, INV,  INV,  INV,  INV,  }, // CMD6
	{INV,  INV,  INV,  TRAN, INV,  INV,  INV,  INV,  PRG,  }, // CMD7 addressed
	//{INV,  INV,  INV,  STBY, STBY, STBY, INV,  DIS,  INV,  },//CMD7 not addressed
	{IDLE, INV,  INV,  INV,  INV,  INV,  INV,  INV,  INV,  }, // CMD8
	{INV,  INV,  INV,  STBY, INV,  INV,  INV,  INV,  INV,  }, // CMD9
	{INV,  INV,  INV,  STBY, INV,  INV,  INV,  INV,  INV,  }, // CMD10
	RESERVED_CMD,                         // CMD11
	{INV,  INV,  INV,  INV,  INV,  TRAN, PRG,  INV,  INV,  }, // CMD12
	{INV,  INV,  INV,  STBY, TRAN, DATA, RCV,  PRG,  DIS,  }, // CMD13
	RESERVED_CMD,                         // CMD14
	{INV,  INV,  INV,  INA,  INA,  INA,  INA,  INA,  INA,  }, // CMD15
	// class 2
	{INV,  INV,  INV,  INV,  TRAN, INV,  INV,  INV,  INV,  }, // CMD16
	{INV,  INV,  INV,  INV,  DATA, INV,  INV,  INV,  INV,  }, // CMD17
	{INV,  INV,  INV,  INV,  DATA, INV,  INV,  INV,  INV,  }, // CMD18
	RESERVED_CMD,                         // CMD19
	RESERVED_CMD,                         // CMD20
	RESERVED_CMD,                         // CMD21
	{INV,  INV,  INV,  INV,  DATA, INV,  INV,  INV,  INV,  }, // ACMD22
	{INV,  INV,  INV,  INV,  TRAN, INV,  INV,  INV,  INV,  }, // ACMD23
	{INV,  INV,  INV,  INV,  RCV,  INV,  INV,  INV,  INV,  }, // CMD24
	{INV,  INV,  INV,  INV,  RCV,  INV,  INV,  INV,  INV,  }, // CMD25
	RESERVED_CMD,                         // CMD26
	{INV,  INV,  INV,  INV,  RCV,  INV,  INV,  INV,  INV,  }, // CMD27
	// class 6
	{INV,  INV,  INV,  INV,  PRG,  INV,  INV,  INV,  INV,  }, // CMD28
	{INV,  INV,  INV,  INV,  PRG,  INV,  INV,  INV,  INV,  }, // CMD29
	{INV,  INV,  INV,  INV,  DATA, INV,  INV,  INV,  INV,  }, // CMD30
	RESERVED_CMD,                         // CMD31
	// class 5
	{INV,  INV,  INV,  INV,  TRAN, INV,  INV,  INV,  INV,  }, // CMD32
	{INV,  INV,  INV,  INV,  TRAN, INV,  INV,  INV,  INV,  }, // CMD33
	RESERVED_CMD,                         // CMD34
	RESERVED_CMD,                         // CMD35
	RESERVED_CMD,                         // CMD36
	RESERVED_CMD,                         // CMD37
	{INV,  INV,  INV,  INV,  PRG,  INV,  INV,  INV,  INV,  }, // CMD38
	RESERVED_CMD,                         // CMD39
	RESERVED_CMD,                         // CMD40
	{REDY, INV,  INV,  INV,  INV,  INV,  INV,  INV,  INV,  }, // ACMD41
	// class 7
	{INV,  INV,  INV,  INV,  RCV,  INV,  INV,  INV,  INV,  }, // CMD42
	RESERVED_CMD,                         // CMD43
	RESERVED_CMD,                         // CMD44
	RESERVED_CMD,                         // CMD45
	RESERVED_CMD,                         // CMD46
	RESERVED_CMD,                         // CMD47
	RESERVED_CMD,                         // CMD48
	RESERVED_CMD,                         // CMD49
	RESERVED_CMD,                         // CMD50,
	{INV,  INV,  INV,  INV,  DATA, INV,  INV,  INV,  INV,  }, // ACMD51,
	RESERVED_CMD,                         // CMD52,
	RESERVED_CMD,                         // CMD53,
	RESERVED_CMD,                         // CMD54,
	// class 8
	{IDLE, INV,  INV,  STBY, TRAN, DATA, RCV,  PRG,  DIS,  }, // CMD55
	{INV,  INV,  INV,  INV,  DATA, INV,  INV,  INV,  INV,  }, // CMD56
	RESERVED_CMD,                         // CMD57,
	RESERVED_CMD,                         // CMD58,
	RESERVED_CMD,                         // CMD59,
	RESERVED_CMD,                         // CMD60,
	RESERVED_CMD,                         // CMD61,
	RESERVED_CMD,                         // CMD62,
	RESERVED_CMD,                         // CMD63,
};

static kvp_t card_state_name[] = {
	{SDP_IDLE,  "card identification mode(Idle State)"},
	{SDP_READY, "card identification mode(Ready State)"},
	{SDP_IDENT, "card identification mode(Identification State)"},
	{SDP_STBY,  "data transfer mode(Stand-by State)"},
	{SDP_TRAN,  "data transfer mode(Transfer State)"},
	{SDP_DATA,  "data transfer mode(Sending-data State)"},
	{SDP_RCV,   "data transfer mode(Receive-data State)"},
	{SDP_PRG,   "data transfer mode(Programming State)"},
	{SDP_DIS,   "data transfer mode(Disconnect State)"},
};

/*----- Extern function implementations ------------------------------*/

sdp_cur_stat_t sdprot_next_stat(int cmd_nr, u8 cur_stat) {
	if (cmd_nr >= countof(card_stat_trans_table)) return SDP_INV;
	if (cur_stat >= SDP_CNT) return SDP_INV;
	return card_stat_trans_table[cmd_nr][cur_stat];
}

const char* sdprot_stat_name(int stat) {
	if (0 <= stat && stat < SDP_CNT) {
		return card_state_name[stat].val;
	}
	if (stat == SDP_INA) {
		return "inactive(Inactive State)";
	}
	return "Invalid Card State";
}

sdcard_response_t sdprot_resp_type(int cmd_nr) {

	switch(cmd_nr) {
	case CMD0_GO_IDLE_STATE:
	case CMD4_SEND_DSR:
	case CMD15_GO_INACTIVE_STATE:
		return SDCARD_NONE_RSP;

	case CMD2R2_ALL_SEND_CID:
	case CMD9R2_SEND_CSD:
	case CMD10R2_SEND_CID:
		return SDCARD_136BIT_RSP;
	default:
		break;
	}

	return SDCARD_48BIT_RSP;
}

int sdprot_resp_crc(int cmd_nr) {
	if (cmd_nr == ACMD41R3_SD_SEND_OP_COND
	||  cmd_nr == CMD1R3_SEND_OP_COND) {
		return 0;
	}
	return 1;
}

sdprot_datadir_t sdprot_need_data(int cmd_nr) {
	switch(cmd_nr) {
	default:
		return SDPROT_NO_DATA;
	case CMD17R1_READ_SINGLE_BLOCK:
	case CMD18R1_READ_MULTIPLE_BLOCK:
	case CMD30R1_SEND_WRITE_PROT:
	case CMD56R1_GEN_CMD:
	case ACMD13R1_SD_STATUS:
	case ACMD22R1_SEND_NUM_WR_BLOCKS:
	case ACMD51R1_SEND_SCR:
		return SDPROT_READ_DATA;
	case CMD24R1_WRITE_BLOCK:
	case CMD25R1_WRITE_MULTIPLE_BLOCK:
	case CMD27R1_PROGRAM_CSD:
	case CMD42R1_LOCK_UNLOCK:
		return SDPROT_WRITE_DATA;
	}
	return SDPROT_NO_DATA;
}

int sdprot_need_busy(int cmd_nr) {
	switch(cmd_nr) {
	default:
		return 0;
	case CMD7R1b_SEL_UNSEL_CARD:
	case CMD8R7_SEND_IF_COND:
	case CMD12R1b_STOP_TRANSMISSION:
	case CMD28R1b_SET_WRITE_PROT:
	case CMD29R1b_CLR_WRITE_PROT:
	case CMD38R1b_ERASE:
		return 1;
	}
	return 0;   
}

int sdprot_get_cid(CID_t* cid, const u32* resp) {
	cid->CRC = __field_xget(resp[0], 0x7F << 1);
	cid->MDT = __field_xget(resp[0], 0xFFF << 8);
	cid->PSN = __field_xget(resp[0], 0xFF << 24) | __field_xget(resp[1], 0xFFFFFF << 0);
	cid->PRV = __field_xget(resp[1], 0xFF << 24);

	memcpy((char*)cid->PNM, (char*)(&resp[2]), 5);
	cid->PNM[5] = 0;

	cid->OID = __field_xget(resp[3], 0xFFFF << 8);
	cid->MID = __field_xget(resp[3], 0xFF << 24);

	return 0;
}

int sdprot_print_cid(const CID_t* cid) {

	DLOG_SD("************ CID register ************\n");
	DLOG_SD("Manufacturer ID MID      \t0x%.2X\n", cid->MID);
	DLOG_SD("OEM/Application ID OID   \t0x%.4X\n", cid->OID);
	DLOG_SD("Product name PNM         \t%s\n",     cid->PNM);
	DLOG_SD("Product revision PRV     \t%d\n",     cid->PRV);
	DLOG_SD("Product serial number PSN\t0x%.8X\n", cid->PSN);
	DLOG_SD("Manufacturing date MDT   \t0x%.3X\n", cid->MDT);
	DLOG_SD("CRC7 checksum CRC        \t0x%.2X\n", cid->CRC);
	DLOG_SD("**************************************\n");

	return 0;
}

int sdprot_get_csd(CSD_t* csd, const u32* resp) {
	u32 v;

	csd->CSD_STRUCTURE = __field_xget(resp[3], 0x3 << 30);
	csd->TAAC = __field_xget(resp[3], 0xFF << 16);
	csd->NSAC = __field_xget(resp[3], 0xFF << 8);
	csd->TRANS_SPEED = __field_xget(resp[3], 0xFF << 0);

	csd->CCC = __field_xget(resp[2], 0xFFF << 20);
	csd->READ_BL_LEN = __field_xget(resp[2], 0xF << 16);
	csd->READ_BL_PARTIAL = __field_xget(resp[2], 0x1 << 15);
	csd->WRITE_BLK_MISALIGN = __field_xget(resp[2], 0x1 << 14);
	csd->READ_BLK_MISALIGN = __field_xget(resp[2], 0x1 << 13);
	csd->DSR_IMP = __field_xget(resp[2], 0x1 << 12);
	if (csd->CSD_STRUCTURE == 1u) {
		csd->C_SIZE =
			(__field_xget(resp[2], 0x3fu) << 16) |
			__field_xget(resp[1], 0xffffu << 16);
	} else {
		v = __field_xget(resp[2], 0x3ffu);
		csd->C_SIZE =
			(v << 2) | __field_xget(resp[1], 0x3u << 30);
	}
	csd->VDD_R_CURR_MIN = __field_xget(resp[1], 0x7 << 27);
	csd->VDD_R_CURR_MAX = __field_xget(resp[1], 0x7 << 24);
	csd->VDD_W_CURR_MIN = __field_xget(resp[1], 0x7 << 21);
	csd->VDD_W_CURR_MAX = __field_xget(resp[1], 0x7 << 18);
	csd->C_SIZE_MULT = __field_xget(resp[1], 0x7 << 15);
	csd->ERASE_BLK_LEN = __field_xget(resp[1], 0x1 << 14);
	csd->SECTOR_SIZE = __field_xget(resp[1], 0x7F << 7);
	csd->WP_GRP_SIZE = __field_xget(resp[1], 0x7F << 0);

	csd->WP_GRP_ENABLE = __field_xget(resp[0], 0x1 << 31);
	csd->R2W_FACTOR = __field_xget(resp[0], 0x7 << 26);
	csd->WRITE_BL_LEN = __field_xget(resp[0], 0xF << 22);
	csd->WRITE_BL_PARTIAL = __field_xget(resp[0], 0x1 << 21);
	csd->FILE_FORMAT_GRP = __field_xget(resp[0], 0x1 << 15);
	csd->COPY = __field_xget(resp[0], 0x1 << 14);
	csd->PERM_WRITE_PROTECT = __field_xget(resp[0], 0x1 << 12);
	csd->TMP_WRITE_PROTECT = __field_xget(resp[0], 0x1 << 11);
	csd->FILE_FORMAT = __field_xget(resp[0], 0x3 << 9);
	csd->CRC = __field_xget(resp[0], 0x7F << 1);
	return 0;
}

int sdprot_print_csd(const CSD_t* csd) {
	DLOG_SD("************ CSD register ************\n");
	DLOG_SD("CSD structure              \t0x%X\n", csd->CSD_STRUCTURE);
	DLOG_SD("data read access-time-1    \t0x%X\n", csd->TAAC);
	DLOG_SD("data read access-time-2    \t0x%X\n", csd->NSAC);
	DLOG_SD("max. data transfer rate    \t0x%X\n", csd->TRANS_SPEED);
	DLOG_SD("card command classes       \t0x%X\n", csd->CCC);
	DLOG_SD("max. read data block length\t0x%X\n", csd->READ_BL_LEN);
	DLOG_SD("partial blocks for read allowed\t0x%X\n", csd->READ_BL_PARTIAL);
	DLOG_SD("write block misalignment   \t0x%X\n", csd->WRITE_BLK_MISALIGN);
	DLOG_SD("read block misalignment    \t0x%X\n", csd->READ_BLK_MISALIGN);
	DLOG_SD("DSR implemented            \t0x%X\n", csd->DSR_IMP);
	DLOG_SD("device size                \t0x%X\n", csd->C_SIZE);
	DLOG_SD("max. read current @VDD min \t0x%X\n", csd->VDD_R_CURR_MIN);
	DLOG_SD("max. read current @VDD max \t0x%X\n", csd->VDD_R_CURR_MAX);
	DLOG_SD("max. write current @VDD min\t0x%X\n", csd->VDD_W_CURR_MIN);
	DLOG_SD("max. write current @VDD max\t0x%X\n", csd->VDD_W_CURR_MAX);
	DLOG_SD("device size multiplier     \t0x%X\n", csd->C_SIZE_MULT);
	DLOG_SD("erase single block enable  \t0x%X\n", csd->ERASE_BLK_LEN);
	DLOG_SD("erase sector size          \t0x%X\n", csd->SECTOR_SIZE);
	DLOG_SD("write protect group size   \t0x%X\n", csd->WP_GRP_SIZE);
	DLOG_SD("write protect group enable \t0x%X\n", csd->WP_GRP_ENABLE);
	DLOG_SD("write speed factor         \t0x%X\n", csd->R2W_FACTOR);
	DLOG_SD("max. write data block length\t0x%X\n", csd->WRITE_BL_LEN);
	DLOG_SD("partial blocks f write allowed\t0x%X\n", csd->WRITE_BL_PARTIAL);
	DLOG_SD("File format group          \t0x%X\n", csd->FILE_FORMAT_GRP);
	DLOG_SD("copy flag (OTP)            \t0x%X\n", csd->COPY);
	DLOG_SD("permanent write protection \t0x%X\n", csd->PERM_WRITE_PROTECT);
	DLOG_SD("temporary write protection \t0x%X\n", csd->TMP_WRITE_PROTECT);
	DLOG_SD("File format                \t0x%X\n", csd->FILE_FORMAT);
	DLOG_SD("CRC                        \t0x%X\n", csd->CRC);
	DLOG_SD("**************************************\n");
	return 0;
}

u32 sdprot_trans_speed(const CSD_t* csd) {
	// 5.3.2 CSD Register
	const u32 muls[] = {
		0, 10, 12, 13,
		15, 20, 25, 30,
		35, 40, 45, 50,
		55, 60, 70, 80,
	};
	u32 v, base, idx, speed;

	v = csd->TRANS_SPEED;

	switch(v & 0x07) {
	case 0: base = 100000; break;
	case 1: base = 1000000; break;
	case 2: base = 10000000; break;
	case 3: base = 100000000; break;
	default: return 0;
	}

	idx = __field_xget(v, 0x78);
	speed = base / 10 * muls[idx];

	DLOG_SD("SDPROT\tTRANS_SPEED = %d * (%d / 10) = %d Hz\n", base, muls[idx], speed);

	return speed;
}

// unit: mA
static const u16 vdd_rw_current_min[] = {
	0/*0.5*/, 1, 5, 10,
	25, 35, 60, 100,
};

// uint: mA
static const u16 vdd_rw_current_max[] = {
	1, 5, 10, 25,
	35, 45, 80, 200,
};

// unit: bytes
u32 sdprot_device_size(const CSD_t* csd) {
	u32 sector_count = sdprot_sector_count(csd);
	u32 max_sectors = 0xffffffffu / SDPROT_SECTOR_SIZE;

	if (sector_count > max_sectors) {
		return 0xffffffffu;
	}

	DLOG_SD("SDPROT\twrite data block length: %d bytes\n", 1 << csd->WRITE_BL_LEN);
	DLOG_SD("SDPROT\tread  data block length: %d bytes\n", 1 << csd->READ_BL_LEN);
	DLOG_SD("SDPROT\tsector number: %u sectors\n", sector_count);
	DLOG_SD("SDPROT\tread current: %dmA %dmA\n", 
		vdd_rw_current_min[csd->VDD_R_CURR_MIN],
		vdd_rw_current_max[csd->VDD_R_CURR_MAX]);
	DLOG_SD("SDPROT\twrite current: %dmA %dmA\n", 
		vdd_rw_current_min[csd->VDD_W_CURR_MIN],
		vdd_rw_current_max[csd->VDD_W_CURR_MAX]);

	return sector_count * SDPROT_SECTOR_SIZE;
}

u32 sdprot_sector_count(const CSD_t* csd) {
	u64 sector_count;

	if (csd == NULL) {
		return 0;
	}

	if (csd->CSD_STRUCTURE == 1u) {
		sector_count = ((u64)csd->C_SIZE + 1u) << 10;
	} else if (csd->CSD_STRUCTURE == 0u) {
		u64 block_count =
			((u64)csd->C_SIZE + 1u) << (csd->C_SIZE_MULT + 2u);
		u64 block_size = (u64)1u << csd->READ_BL_LEN;

		sector_count =
			(block_count * block_size) / SDPROT_SECTOR_SIZE;
	} else {
		return 0;
	}

	if (sector_count > 0xffffffffu) {
		return 0xffffffffu;
	}

	return (u32)sector_count;
}

int sdprot_print_r1_stat(const sdp_r1_stat_t* r1_stat) {
	DLOG_SD("************ R1 response  ************\n");
	DLOG_SD("RES_TEST_MODE    \t=0x%X\n", r1_stat->RES_TEST_MODE);
	DLOG_SD("RES_APP_CMD      \t=0x%X\n", r1_stat->RES_APP_CMD);
	DLOG_SD("AKE_SEQ_ERROR    \t=0x%X\n", r1_stat->AKE_SEQ_ERROR);
	DLOG_SD("RES_SDIO_CARD    \t=0x%X\n", r1_stat->RES_SDIO_CARD);
	DLOG_SD("APP_CMD          \t=0x%X\n", r1_stat->APP_CMD);
	DLOG_SD("RES1             \t=0x%X\n", r1_stat->RES1);
	DLOG_SD("READY_FOR_DATA   \t=0x%X\n", r1_stat->READY_FOR_DATA);
	DLOG_SD("CURRENT_STATE    \t=%s\n", sdprot_stat_name(r1_stat->CURRENT_STATE));
	DLOG_SD("ERASE_RESET      \t=0x%X\n", r1_stat->ERASE_RESET);
	DLOG_SD("CARD_ECC_DISABLED\t=0x%X\n", r1_stat->CARD_ECC_DISABLED);
	DLOG_SD("WP_ERASE_SKIP    \t=0x%X\n", r1_stat->WP_ERASE_SKIP);
	DLOG_SD("CSD_OVERWRITE    \t=0x%X\n", r1_stat->CSD_OVERWRITE);
	DLOG_SD("RES2             \t=0x%X\n", r1_stat->RES2);
	DLOG_SD("ERROR            \t=0x%X\n", r1_stat->ERROR);
	DLOG_SD("CC_ERROR         \t=0x%X\n", r1_stat->CC_ERROR);
	DLOG_SD("CARD_ECC_FAILED  \t=0x%X\n", r1_stat->CARD_ECC_FAILED);
	DLOG_SD("ILLEGAL_COMMAND  \t=0x%X\n", r1_stat->ILLEGAL_COMMAND);
	DLOG_SD("COM_CRC_ERROR    \t=0x%X\n", r1_stat->COM_CRC_ERROR);
	DLOG_SD("CARD_UNLOCK_FAILED\t=0x%X\n", r1_stat->CARD_UNLOCK_FAILED);
	DLOG_SD("CARD_IS_LOCKED   \t=0x%X\n", r1_stat->CARD_IS_LOCKED);
	DLOG_SD("WP_VIOLATION     \t=0x%X\n", r1_stat->WP_VIOLATION);
	DLOG_SD("ERASE_PARAM      \t=0x%X\n", r1_stat->ERASE_PARAM);
	DLOG_SD("ERASE_SEQ_ERROR  \t=0x%X\n", r1_stat->ERASE_SEQ_ERROR);
	DLOG_SD("BLOCK_LEN_ERROR  \t=0x%X\n", r1_stat->BLOCK_LEN_ERROR);
	DLOG_SD("ADDRESS_ERROR    \t=0x%X\n", r1_stat->ADDRESS_ERROR);
	DLOG_SD("OUT_OF_RANGE     \t=0x%X\n", r1_stat->OUT_OF_RANGE);
	DLOG_SD("**************************************\n");

	return 0;
}
