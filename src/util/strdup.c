/* File: src/util/strdup.c
 * Part of cards <github.com/rmkrupp/cards>
 *
 * Copyright (C) 2024 Noah Santer <n.ed.santer@gmail.com>
 * Copyright (C) 2024 Rebecca Krupp <beka.krupp@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "util/strdup.h"

#include <stdlib.h>
#include <string.h>

[[nodiscard]] char * util_strdup(const char * s)
{
#if defined(__linux__)
    return strdup(s);
#elif defined(__MINGW32__)
    return _strdup(s);
#else
#error unsupported platform (no util_strdup)
#endif
}

[[nodiscard]] char * util_strndup(const char * s, size_t n)
{
#if defined(__linux__)
    return strndup(s, n);
#else
    char * out = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        out[i] = s[i];
        if (!s[i]) {
            return out;
        }
    }
    out[n] = '\0';
    return out;
#endif
}

