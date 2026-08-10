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
 * @file    ft_macros.h
 *
 * @brief   Macros used throughout Freetribe source code
 */

#ifndef FT_MACROS_H
#define FT_MACROS_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/


/*----- Macros -------------------------------------------------------*/

//
// Debugging helpers
//
#ifdef DEBUG
#   ifndef BLACKFIN
#      include "freetribe.h" // ft_print, ft_printf
#   endif
#   define DEBUG_LOG(fmt, ...)  ft_printf(fmt, ##__VA_ARGS__)
#else
#   define DEBUG_LOG(...)
#endif

#ifdef BLACKFIN

  #define STATIC_ASSERT(cond, msg) \
      typedef char static_assertion_##msg[(cond) ? 1 : -1]

#else

  #define ASSERT_ERROR_FORMAT "Assertion failed: %s, file %s, line %d\n"

  #define ASSERT(condition)                                                   \
      do {                                                                    \
          if (!(condition)) {                                                 \
              fatal_error(                                                    \
                  ASSERT_ERROR_FORMAT,                                        \
                  #condition, __FILE__, __LINE__);                            \
          }                                                                   \
      } while (0)

#endif

// Bit operations
#define HI16(x)              ((uint16_t)((((uint32_t)(x)) >> 16) & 0xFFFF))
#define LO16(x)              ((uint16_t)(((uint32_t)(x)) & 0xFFFF))
#define COMBINE16(high, low) (((uint32_t)(high) << 16) | ((uint32_t)(low) & 0xFFFF))

// `*(to_struct*)&my_struct` is undefined behavior with strict aliasing
#define STRUCT_CAST(to_type, from_val) \
    ((union { __typeof__(from_val) f; to_type t; }){ .f = (from_val) }).t

// Math helpers
#ifndef MIN
#   define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif

#ifndef offsetof
#  define offsetof(type, member) ((size_t) &(((type *)0)->member))
#endif

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
