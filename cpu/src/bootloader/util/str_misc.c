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
 * @file    str_misc.c
 * 
 * @brief   Miscellaneous C-string functions
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#include "ft.h"

#include <string.h>

#include "str_misc.h"


static const char s_hexchars[] = "0123456789abcdef";


char char_to_upper(char c)
{
	if ((c >= 'a') && (c <= 'z')) {
		return c - ('a' - 'A');
	}
	return c;

}

/**
 * @brief   Convert ch from a hex digit to an int.
 */
int hex_nibble(u8 ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}



char hex_digit(u8 value)
{
	return s_hexchars[value & 0x0F];
}

void hex_digits32(char *out, u32 value)
{
	out[0] = s_hexchars[(value >>  4) & 0x0Fu];
	out[1] = s_hexchars[ value        & 0x0Fu];
	out[2] = s_hexchars[(value >> 12) & 0x0Fu];
	out[3] = s_hexchars[(value >>  8) & 0x0Fu];
	out[4] = s_hexchars[(value >> 20) & 0x0Fu];
	out[5] = s_hexchars[(value >> 16) & 0x0Fu];
	out[6] = s_hexchars[(value >> 28)        ];
	out[7] = s_hexchars[(value >> 24) & 0x0Fu];
}




bool buf_starts_with_cstr(
	const u8   *buf,
	size_t      buflen,
	const char *prefix
) {
	for (size_t i = 0; i < buflen; ++i) {
		if (prefix[i] == '\0')
			return true;

		if (buf[i] != (u8)prefix[i])
			return false;
	}

	return prefix[buflen] == '\0';
}
