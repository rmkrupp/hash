/* File: src/test/test.c
 * Part of hash <github.com/rmkrupp/hash>
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
#include "hash.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;

    struct hash_inputs * hash_inputs = hash_inputs_create();
    hash_inputs_add(hash_inputs, "foo", 3, NULL);
    hash_inputs_add(hash_inputs, "bar", 3, NULL);
    hash_inputs_add(hash_inputs, "donkey", 6, NULL);
    hash_inputs_add(hash_inputs, "mineral", 7, NULL);
    hash_inputs_add(hash_inputs, "toaster oven", 12, NULL);

    size_t length = 8;
    char * s = malloc(length + 1);
    s[length] = '\0';
    for (size_t i = 0; i < 100; i++) {
        for (size_t j = 0; j < length; j++) {
            s[j] = 'a' + rand() % 26;
        }
        hash_inputs_add_safe(hash_inputs, s, strlen(s), NULL);
    }
    free(s);

    struct hash_inputs_statistics hash_inputs_statistics;
    hash_inputs_get_statistics(hash_inputs, &hash_inputs_statistics);
    printf("[instats] n_growths = %lu\n", hash_inputs_statistics.n_growths);
    printf("[instats] capacity = %lu\n", hash_inputs_statistics.capacity);

    struct hash * hash = hash_create(hash_inputs);
    hash_inputs_destroy(hash_inputs);

    if (!hash) {
        printf("hash is null\n");
        return 1;
    }

    const struct hash_lookup_result * result1 = hash_lookup(hash, "mineral", 7);
    if (result1) {
        printf("%s\n", result1->key);
    } else {
        printf("result1 is null\n");
    }

    const struct hash_lookup_result * result2 = hash_lookup(hash, "gronk", 5);
    if (result2) {
        printf("%s\n", result2->key);
    } else {
        printf("result2 is null\n");
    }

    hash_destroy(hash);
}
