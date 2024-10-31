/* File: include/hash/hash.h
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
#ifndef HASH_H
#define HASH_H

#include <stddef.h>

/* a hash table */
struct hash;

/* a list of inputs to create a hash_table with */
struct hash_inputs;

/* the result of a hash_lookup[() */
struct hash_lookup_result {
    const char * key;
    size_t n;
    void * ptr;
};

/* calculate a hash table for all the elements in from */
[[nodiscard]] struct hash * hash_create(
        struct hash_inputs * from) [[gnu::nonnull(1)]];

/* destroy this hash table */
void hash_destroy(struct hash * hash) [[gnu::nonnull(1)]];

/* apply this function over every element of this hash */
void hash_apply(
        struct hash * hash,
        void (*fn)(const char * s, size_t n, void ** ptr)
    ) [[gnu::nonnull(1, 2)]];

/* look up this key of length n in this hash,
 * returning a const pointer to the result if found or NULL otherwise
 */
const struct hash_lookup_result * hash_lookup(
        struct hash * hash,
        const char * key,
        size_t n
    ) [[gnu::nonnull(1, 2)]];


/* the statistics filled by hash_get_statistics */
struct hash_statistics {

};


/* fill statistics with statistics on this hash
 * these statistics will only be accurate if hash.c was compiled with
 * -DHASH_STATISTICS
 */
void hash_get_statistics(
        struct hash * hash,
        struct hash_statistics * statistics
    ) [[gnu::nonnull(1, 2)]];

/* create a hash_inputs structure, optionally filling it with all the elements
 * that were in from (or no elements if from is NULL)
 */
[[nodiscard]] struct hash_inputs * hash_inputs_create(struct hash * from);

/* destroy a hash_inputs structure */
void hash_inputs_destroy(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]];

/* grow the capacity of hash_inputs by n */
void hash_inputs_grow(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]];

/* pre-allocate space in hash_inputs for at least n inputs */
void hash_inputs_at_least(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]];

/* add this string of length n to this hash_inputs, associating it with ptr */
void hash_inputs_add(
        struct hash_inputs * hash_inputs,
        const char * s,
        size_t n,
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

/* apply this function over every input*/
void hash_inputs_apply(
        struct hash_inputs * hash_inputs,
        void (*fn)(const char * s, size_t n, void ** ptr)
    ) [[gnu::nonnull(1, 2)]];

/* the statistics filled by hash_inputs_get_statistics */
struct hash_inputs_statistics {
    size_t n_growths;
    size_t capacity;
};

/* fill statistics with statistics on this hash_inputs
 * these statistics will only be accurate if hash.c was compiled with
 * -DHASH_STATISTICS
 */
void hash_inputs_get_statistics(
        struct hash_inputs * hash_inputs,
        struct hash_inputs_statistics * statistics
    ) [[gnu::nonnull(1, 2)]];

#endif /* HASH_H */
