#ifndef PROGRAM_LOADER_H
#define PROGRAM_LOADER_H

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

/*--------------------------------------------------------------------*/

#define MAX_LOAD_SEGMENTS    16

struct program {
	u32 entry;
	u32 num_segments;
	struct segment {
		u32 foffset;
		u32 vaddr;
		u32 file_size;
		u32 mem_size;
	} segments[MAX_LOAD_SEGMENTS];
};

/** @returns u32 number of bytes read */
typedef u32 (*progldr_read_bytes_cb_t)(void *dst, u32 src, u32 nbytes);

struct progldr {
	progldr_read_bytes_cb_t read_bytes_cb;
	u32                     source_size;
};

typedef enum {
	PROGRAM_OK = 0,
	PROGRAM_VALID_ELF,
	PROGRAM_VALID_VSB,
	PROGRAM_ERR_READ,
	PROGRAM_ERR_INVALID,
} progldr_status_t;

/*--------------------------------------------------------------------*/

progldr_status_t program_identify(struct progldr *ldr, struct program *prog);
progldr_status_t program_load(struct progldr *ldr, const struct program *prog);
void program_boot(const struct program *prog) __attribute__((noreturn));

/*--------------------------------------------------------------------*/

#endif /* PROGRAM_LOADER_H */
/*----- End of file --------------------------------------------------*/
