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
 * @file    debug.c
 * 
 * @brief   Provides an interface for debug printing to GDB with string-
 *          formatting.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <stdarg.h>
#include <string.h>

#define TINYPRINTF_DEFINE_TFP_PRINTF 0
#define TINYPRINTF_OVERRIDE_LIBC 0
#include <tinyprintf.h>

#include "arm_frame.h"
#include "dev_lcd.h"
#include "ui/render.h"
#include "util/pathstr.h"
#include "debug.h"

/*----- Macros -------------------------------------------------------*/

#define MAX_STR_LEN 512

/*----- Static variable definitions ----------------------------------*/

static char s_str[MAX_STR_LEN];

volatile arm_frame_t g_fatal_frame;
volatile u32 g_fatal_active;

/*----- Static function prototypes -----------------------------------*/

static void _debug_print_frame(const volatile arm_frame_t *frame);

/*----- Extern function implementations ------------------------------*/

void debug_print_sh(const char *str)
{
	asm volatile(
		"stmdb sp!, {r4, lr}\n"
		"mov r0, %0\n"
		"mov r1, %1\n"
		"swi 0x123456\n"
		"ldmia sp!, {r4, lr}\n"
		:
		: "r"(SH_SYS_WRITE0), "r"(str)
		: "r0", "r1", "memory"
	);
}

// /**
//  * @brief   Overrides freetribe kernel ft_printf called by DLOG macro.
//  */
// void ft_printf(const char *format, ...) {
// 	va_list ap;

// 	va_start(ap, format);
// 	tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
	
// 	debug_print_sh(s_str);
	
// 	va_end(ap);
// }


// /**
//  *  @brief   TinyUSB printf wrapper. Prevents stdlib/newlib integration, which
//  *           costed too much space in the binary.
//  */
// int boot_tusb_printf(const char *format, ...) {
// 	va_list ap;

// 	va_start(ap, format);
// 	tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
	
// 	debug_print_sh(s_str);
	
// 	va_end(ap);
// 	return 0;
// }

/**
 * @brief  Bootloader fatal error trap function
 */
void fatal_error_impl(const char *format, ...)
	__attribute__((noreturn, format(printf, 1, 2)));

/// @TODO: execute from safe stack context (in case of stack overflow)
///        use static piece of memory (ARM RAM?) for stack ?
void fatal_error_impl(const char *format, ...)
{
	va_list ap;
	va_list assertion_ap;
	const char *assertion_condition = NULL;
	const char *assertion_file = NULL;
	int assertion_line = 0;
	bool is_assertion;

	dev_lcd_set_backlight(false, false, true);

	if (!format) {
		format = "(no message)";
	}

	va_start(ap, format);
	is_assertion = 0 == strcmp(format, ASSERT_ERROR_FORMAT);
	if (is_assertion) {
		va_copy(assertion_ap, ap);
		assertion_condition = va_arg(assertion_ap, const char *);
		assertion_file = va_arg(assertion_ap, const char *);
		assertion_line = va_arg(assertion_ap, int);
		va_end(assertion_ap);
	}
	tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
	va_end(ap);

	debug_print_sh("\nFATAL ERROR\n");
	debug_print_sh(s_str);
	debug_print_sh("\n");
	_debug_print_frame(&g_fatal_frame);

	if (is_assertion) {
		tfp_snprintf(
			s_str,
			MAX_STR_LEN,
			"Assertion failed:\n%s\n%s:%d",
			assertion_condition,
			path_extract_filename(assertion_file),
			assertion_line
		);
	}
	(void)gui_render_fatal(s_str, &g_fatal_frame);

	while (true) {
		asm volatile ("" ::: "memory");
	}
}




/*----- Static function implementations ------------------------------*/

static void _debug_print_frame(const volatile arm_frame_t *frame)
{
	char line[80];

	tfp_snprintf(
		line,
		sizeof(line),
		"R0=%08X R1=%08X R2=%08X R3=%08X\n",
		(unsigned int)frame->r0,
		(unsigned int)frame->r1,
		(unsigned int)frame->r2,
		(unsigned int)frame->r3
	);
	debug_print_sh(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"R4=%08X R5=%08X R6=%08X R7=%08X\n",
		(unsigned int)frame->r4,
		(unsigned int)frame->r5,
		(unsigned int)frame->r6,
		(unsigned int)frame->r7
	);
	debug_print_sh(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"R8=%08X R9=%08X R10=%08X R11=%08X\n",
		(unsigned int)frame->r8,
		(unsigned int)frame->r9,
		(unsigned int)frame->r10,
		(unsigned int)frame->r11
	);
	debug_print_sh(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"R12=%08X SP=%08X LR=%08X PC=%08X\n",
		(unsigned int)frame->r12,
		(unsigned int)frame->sp,
		(unsigned int)frame->lr,
		(unsigned int)frame->pc
	);
	debug_print_sh(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"CPSR=%08X\n",
		(unsigned int)frame->cpsr
	);
	debug_print_sh(line);
}

