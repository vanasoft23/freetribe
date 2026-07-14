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

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdarg.h>
#define TINYPRINTF_DEFINE_TFP_PRINTF 0
#define TINYPRINTF_OVERRIDE_LIBC 0
#include <tinyprintf.h>
#include "dev_lcd.h"

/*----- Macros -------------------------------------------------------*/

#define MAX_STR_LEN 512

#define SH_SYS_WRITE0  0x04  // write null-terminated string
#define SH_SYS_WRITEC  0x03  // write single character

/*----- Static variable definitions ----------------------------------*/

static char s_str[MAX_STR_LEN];

/*----- Extern function implementations ------------------------------*/

/**
 * @brief  Print debug string via semihosting.
 *         Will show up in your GDB console.
 */
inline static void debug_print_sh(const char *str) {
    asm volatile (
        "mov r0, %0\n"
        "mov r1, %1\n"
        "swi 0x123456\n"
        :
        : "r"(SH_SYS_WRITE0), "r"(str)
        : "r0", "r1"
    );
}

/**
 * @brief  Overrides freetribe kernel ft_printf called by DEBUG_LOG macro.
 */
void ft_printf(const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
    
    debug_print_sh(s_str);
    
    va_end(ap);
}

/**
 *  @brief TinyUSB printf wrapper. Prevents stdlib/newlib integration, which
 *         costed too much space in the binary.
 */
int boot_tusb_printf(const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
    
    debug_print_sh(s_str);
    
    va_end(ap);
    return 0;
}

/**
 * @brief  Bootloader fatal error trap function
 */
void error(const char *format, ...) {

    va_list ap;

    va_start(ap, format);
    tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
    debug_print_sh(s_str);
    va_end(ap);

    dev_lcd_set_backlight(true, false, false);

    while(true)
        ; // trap
}