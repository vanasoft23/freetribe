/**
 * @file   Deferred printf, for efficiency, formatting
 *         happens on GDB side.
 *
 * @author vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "ring_buffer.h"

#define TINYPRINTF_DEFINE_TFP_PRINTF 0
#define TINYPRINTF_OVERRIDE_LIBC 0
#include <tinyprintf.h>

#include <stdarg.h>

#include "debug.h"
#include "gdb_stub.h"
#include "svc_logger.h"

/*----- Macros -------------------------------------------------------*/

#define MAX_STR_LEN          512

/*----- Types --------------------------------------------------------*/

/*----- Variables ----------------------------------------------------*/

static char     s_str[MAX_STR_LEN];

/*----- Extern function implementations ------------------------------*/


void svc_log_printf(const char *format, ...)
{
	// @TODO: mutex this

	va_list ap;

	va_start(ap, format);
	tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
	
	gdb_stub_print(s_str);
	debug_print_sh(s_str);
	
	va_end(ap);
}
