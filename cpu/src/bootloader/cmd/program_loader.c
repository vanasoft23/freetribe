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
 * @file    program_loader.c
 *
 * @brief   Program image validation, loading, and handoff.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "cmd/handoff.h"
#include "program_loader.h"

/*--------------------------------------------------------------------*/

extern u8 _ddr2_start;
extern u8 _program_ddr_end;
#define ALLOWED_DDR_START ((u32)(uintptr_t)&_ddr2_start)
#define ALLOWED_DDR_END   ((u32)(uintptr_t)&_program_ddr_end)


#define ELF_HEADER_SIZE 0x34u

static const u8  ELF_IDENT[8] = { 0x7F,0x45,0x4C,0x46, 0x01, 0x01, 0x01, 0x00 };
static const u16 ET_EXEC = 0x0002u;
static const u16 EM_ARM = 0x0028u;
static const u32 PT_LOAD = 0x00000001u;

struct ehdr
{
	u16 type;
	u16 machine;
	u32 version;
	u32 entry;
	u32 phoff;
	u32 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shstrndx;
} __attribute__((packed));
_Static_assert(sizeof(struct ehdr) == 36, "invalid ehdr struct size");

struct phdr
{
	u32 type;
	u32 offset;
	u32 vaddr;
	u32 paddr;
	u32 filesz;
	u32 memsz;
	u32 flags;
	u32 align;
} __attribute__((packed));
_Static_assert(sizeof(struct phdr) == 32, "invalid phdr struct size");


// purposefully very lenient header checks
static const u8 VSB_HEADER_STUB[] = {
    0x4B, 0x4F, 0x52, 0x47, 0x20, 0x53, 0x59, 0x53, 0x54, 0x45, 0x4D, 0x20,
    0x46, 0x49, 0x4C, 0x45, 0x45, 0x32
};
static const u32 VSB_ENTRY_ADDR = 0xC0000000u;


/*----- Static function prototypes -----------------------------------*/

static progldr_status_t _elf_detect(struct progldr *ldr, struct program *prog);
static progldr_status_t _vsb_detect(struct progldr *ldr, struct program *prog);
static bool _allowed_memory_range(u32 address, u32 size);

/*----- Extern function implementations ------------------------------*/

progldr_status_t program_identify(struct progldr *ldr, struct program *prog)
{
	progldr_status_t status;

	if ((NULL == ldr) || (NULL == ldr->read_bytes_cb) || (NULL == prog)) {
		return PROGRAM_ERR_INVALID;
	}

	memset(prog, 0, sizeof(*prog));

	status = _elf_detect(ldr, prog);
	if (PROGRAM_ERR_INVALID != status) {
		return status;
	}

	status = _vsb_detect(ldr, prog);
	if (PROGRAM_ERR_INVALID != status) {
		return status;
	}

	return PROGRAM_ERR_INVALID;
}

progldr_status_t program_load(struct progldr *ldr, const struct program *prog)
{
	if ((NULL == ldr) || (NULL == ldr->read_bytes_cb) || (NULL == prog)) {
		return PROGRAM_ERR_INVALID;
	}

	for (u32 i = 0; i < prog->num_segments; i++) {
		const struct segment *seg = &prog->segments[i];

		if (0 == seg->file_size) {
			continue;
		}

		u32 bytes_read = ldr->read_bytes_cb(
			(void *)(uintptr_t)seg->vaddr,
			seg->foffset,
			seg->file_size
		);
		if (bytes_read != seg->file_size) {
			return PROGRAM_ERR_READ;
		}
	}

	for (u32 i = 0; i < prog->num_segments; i++) {
		const struct segment *seg = &prog->segments[i];

		if (seg->mem_size > seg->file_size) {
			memset(
				(void *)(uintptr_t)(seg->vaddr + seg->file_size),
				0,
				seg->mem_size - seg->file_size
			);
		}
	}

	return PROGRAM_OK;
}

void program_boot(const struct program *prog)
{
	ASSERT(NULL != prog);
	handoff_program(prog->entry);
	for (;;);
}

/*----- Static function implementations ------------------------------*/

static progldr_status_t _elf_detect(struct progldr *ldr, struct program *prog)
{
	u8 scratch[0x40];

	if (ldr->source_size < ELF_HEADER_SIZE) {
		return PROGRAM_ERR_INVALID;
	}

	u32 bytes_read = ldr->read_bytes_cb(scratch, 0, ELF_HEADER_SIZE);
	if (bytes_read != ELF_HEADER_SIZE) {
		return PROGRAM_ERR_READ;
	}

	if (0 != memcmp(scratch, ELF_IDENT, sizeof(ELF_IDENT))) {
		return PROGRAM_ERR_INVALID;
	}

	struct ehdr ehdr;
	memcpy(&ehdr, &scratch[0x10], sizeof(struct ehdr));
	if (ehdr.type       != ET_EXEC
	 || ehdr.machine    != EM_ARM
	 || ehdr.version    != 1u
	 || ehdr.ehsize     != ELF_HEADER_SIZE
	 || ehdr.phentsize  != sizeof(struct phdr)
	 || ehdr.phnum      == 0
	 || ehdr.phoff   > ldr->source_size
	 || ehdr.phnum > ((ldr->source_size - ehdr.phoff) / ehdr.phentsize)) {
		return PROGRAM_ERR_INVALID;
	}

	u32 entry_address = ehdr.entry & ~1u;
	bool thumb_entry = 0 != (ehdr.entry & 1u);
	bool aligned4 = 0 != (entry_address & 3u);
	if ((!thumb_entry && aligned4) || !_allowed_memory_range(entry_address, 1u)) {
		return PROGRAM_ERR_INVALID;
	}

	prog->entry = ehdr.entry;
	prog->num_segments = 0;
	bool entry_is_loaded = false;

	u32 phoff = ehdr.phoff;
	for (u32 i = 0; i < ehdr.phnum; i++) {

		bytes_read = ldr->read_bytes_cb(scratch, phoff, sizeof(struct phdr));
		if (bytes_read != sizeof(struct phdr)) {
			return PROGRAM_ERR_READ;
		}
		phoff += ehdr.phentsize;

		struct phdr *phdr = (struct phdr *)scratch;

		// Does the section need to get loaded?
		if ((phdr->type != PT_LOAD)
		 || (0 == phdr->offset)
		 || (0 == phdr->filesz)
		 || (0 == phdr->memsz)) {
			continue;
		}

		if (prog->num_segments >= MAX_LOAD_SEGMENTS
		 || phdr->filesz > phdr->memsz
		 || phdr->offset > ldr->source_size
		 || phdr->filesz > (ldr->source_size - phdr->offset)
		 || !_allowed_memory_range(phdr->vaddr, phdr->memsz)) {
			return PROGRAM_ERR_INVALID;
		}

		struct segment *segment = &prog->segments[prog->num_segments++];
		segment->foffset   = phdr->offset;
		segment->vaddr     = phdr->vaddr;
		segment->file_size = phdr->filesz;
		segment->mem_size  = phdr->memsz;

		if ((entry_address >= phdr->vaddr)
		 && ((entry_address - phdr->vaddr) < phdr->filesz)) {
			entry_is_loaded = true;
		}
	}

	if ((0 == prog->num_segments) || !entry_is_loaded) {
		return PROGRAM_ERR_INVALID;
	}

	return PROGRAM_VALID_ELF;
}



static progldr_status_t _vsb_detect(struct progldr *ldr, struct program *prog)
{
	u8 scratch[0x40];
	
	u8 bytes_read = ldr->read_bytes_cb(scratch, 0, sizeof(VSB_HEADER_STUB));
	if (bytes_read != sizeof(VSB_HEADER_STUB)) {
		return PROGRAM_ERR_READ;
	}
	if (0 != memcmp(scratch, VSB_HEADER_STUB, sizeof(VSB_HEADER_STUB))) {
		return PROGRAM_ERR_INVALID;
	}
	bool ver_is_sampler = ('S' == scratch[0x12]);

	// Load entire VSB (except header ofcourse) at 0xC0000000
	prog->entry = VSB_ENTRY_ADDR;
	prog->num_segments = 1;
	struct segment *seg = &prog->segments[0];
	seg->foffset   = 0x00000100;
	seg->vaddr     = VSB_ENTRY_ADDR;
	seg->file_size = 0x200000;
	seg->mem_size  = 0x200000;

	return PROGRAM_VALID_VSB;
}

static bool _allowed_memory_range(u32 address, u32 size)
{
	return (address >= ALLOWED_DDR_START)
		&& (address <= ALLOWED_DDR_END)
		&& (size <= (ALLOWED_DDR_END - address));
}

/*----- End of file --------------------------------------------------*/
