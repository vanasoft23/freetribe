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
 * @file    ft_error.h
 */

#ifndef FT_ERROR_H
#define FT_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif


/*----- Macros -------------------------------------------------------*/


#ifdef BLACKFIN

  #ifdef __clang__
    #define STATIC_ASSERT(cond, msg)
  #else
    #define STATIC_ASSERT(cond, msg) \
        typedef char static_assertion_##msg[(cond) ? 1 : -1]
  #endif

#else

  #ifdef DEBUG

    #define ASSERT_ERROR_FORMAT "Assertion failed: %s, file %s, line %d\n"

    #define ASSERT(condition)                                                   \
        do {                                                                    \
            if (!(condition)) {                                                 \
                fatal_error(                                                    \
                    ASSERT_ERROR_FORMAT,                                        \
                    #condition, __FILE__, __LINE__);                            \
            }                                                                   \
        } while (0)

  #else

    #define ASSERT(condition) ((void)0)

  #endif

#endif

/*----- Kernel fatal error handling ----------------------------------*/

typedef enum {
    PANIC_USER             = 0,
    PANIC_GENERIC          = 1,
    PANIC_STACK_CORRUPTION = 2,
    PANIC_EXCEPTION        = 3,
    PANIC_UNHANDLED_STATE  = 4,
} e_panic_codes;

#define PANIC(error_code)                                                   \
    do {                                                                    \
        fatal_error(                                                        \
            "ERROR %d, %s:%d",                                              \
            error_code, __FILE__, __LINE__);                                \
    } while (0)

extern void fatal_error(const char *format, ...)
    __attribute__((noreturn, format(printf, 1, 2)));

/*--------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
#endif /* FT_ERROR_H_ */

/*----- End of file --------------------------------------------------*/