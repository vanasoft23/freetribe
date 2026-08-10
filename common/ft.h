/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

                                License

                   GNU AFFERO GENERAL PUBLIC LICENSE
                      Version 3, 19 November 2007

                           AGPL-3.0-or-later

----------------------------------------------------------------------*/

/**
 * @file    ft.h
 * 
 * @brief   Central common definitions that should be included by every
 *          freetribe source file.
 */

#ifndef FT_H
#define FT_H

#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "ft_types.h"
#include "ft_macros.h"

/// TODO: move into ft_error along with ASSERT and shit
extern void fatal_error(const char *format, ...)
    __attribute__((noreturn, format(printf, 1, 2))); // #include "ft_error.h"

#endif
