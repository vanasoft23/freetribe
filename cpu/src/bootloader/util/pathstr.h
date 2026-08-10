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
 * @file    pathstr.h
 * 
 * @brief   Text and path string functions
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef PATHSTR_H
#define PATHSTR_H

#include "ft.h"


#define PATH_NAME_MAX 256


bool path_has_extension(const char *name, const char *extension);
void path_build_child_path(char *dest, u16 dest_size, const char *name, const char *current_dir);
void path_goto_parent_dir(char *path);

static inline bool path_is_root_dir(const char *path) {
    return 0 == strcmp(path, "/");
}

static inline bool path_is_dot_entry(const char *name) {
    return (0 == strcmp(name, ".")) || (0 == strcmp(name, ".."));
}

#endif /* PATHSTR_H */
