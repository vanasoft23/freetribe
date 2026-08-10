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
 * @file    pathstr.c
 * 
 * @brief   Path cstring functions
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#include "ft.h"
#include "str_misc.h"

#include <string.h>

#include "pathstr.h"


bool path_has_extension(const char *name, const char *extension) {

    u16 name_len = strlen(name);
    u16 ext_len = strlen(extension);

    if (name_len < ext_len) {
        return false;
    }

    name += name_len - ext_len;
    while (*extension) {
        if (char_to_upper(*name) != char_to_upper(*extension)) {
            return false;
        }
        name++;
        extension++;
    }

    return true;

}

void path_build_child_path(char *dest, u16 dest_size, const char *name, const char *current_dir) {

    u16 len;

    memset(dest, 0, dest_size);

    if (path_is_root_dir(current_dir)) {
        strncpy(dest, "/", dest_size - 1);
    } else {
        strncpy(dest, current_dir, dest_size - 1);
        dest[dest_size - 1] = '\0';

        len = strlen(dest);
        if (len < (dest_size - 1)) {
            strncat(dest, "/", dest_size - len - 1);
        }
    }

    len = strlen(dest);
    if (len < (dest_size - 1)) {
        strncat(dest, name, dest_size - len - 1);
    }

}


void path_goto_parent_dir(char *path) {

    // Copy old path string
    char tmp[PATH_NAME_MAX];
    strncpy(tmp, path, PATH_NAME_MAX - 1);
    tmp[PATH_NAME_MAX - 1] = '\0';

    char *last_separator = strrchr(tmp, '/');
    if ((NULL == last_separator) || (last_separator == tmp)) {

        strncpy(path, "/", PATH_NAME_MAX - 1);
    } else {

        *last_separator = '\0';
        strncpy(path, tmp, PATH_NAME_MAX - 1);
    }

    path[PATH_NAME_MAX - 1] = '\0';
}

