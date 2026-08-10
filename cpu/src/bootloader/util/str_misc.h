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
 * @file    str_misc.h
 * 
 * @brief   Miscellaneous C-string functions.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef STR_MISC_H
#define STR_MISC_H

#include "ft.h"


char char_to_upper(char c);

/**
 * @brief   Convert ch from a hex digit to an int.
 */
int hex_nibble(u8 ch);

/**
 * @brief   Convert 8 hex digits to it's corresponding 32-bit value.
 * 
 * @returns true if the digits were valid, false otherwise
 */
bool hex_32frombuf(char *buf, u32 *out_value);
char hex_digit(u8 value);
void hex_digits32(char *out, u32 value);

bool buf_starts_with_cstr(const u8 *buf, size_t buflen, const char *prefix);


#endif /* STR_MISC_H */
