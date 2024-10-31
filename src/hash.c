/* File: src/hash.c
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

#include "util/strdup.h"
#include <assert.h>
#include <stdlib.h>

constexpr size_t hash_inputs_grow_increment = 1;

struct hash {

#ifdef HASH_STATISTICS
    struct hash_statistics statistics;
#endif /* HASH_STATISTICS */
};

struct hash_input {
    char * s;
    size_t n;
    void * ptr;
};

struct hash_inputs {
    struct hash_input * inputs;
    size_t n_inputs;
    size_t capacity;
#ifdef HASH_STATISTICS
    struct hash_inputs_statistics statistics;
#endif /* HASH_STATISTICS */
};

/* calculate a hash table for all the elements in from */
[[nodiscard]] struct hash * hash_create(
        struct hash_inputs * from) [[gnu::nonnull(1)]]
{
    // TODO
    (void)from;
    return NULL;
}

/* destroy this hash table */
void hash_destroy(struct hash * hash) [[gnu::nonnull(1)]]
{
    // TODO
    (void)hash;
}

/* apply this function over every element of this hash */
void hash_apply(
        struct hash * hash,
        void (*fn)(const char * s, size_t n, void ** ptr)
    ) [[gnu::nonnull(1, 2)]]
{
    // TODO
    (void)hash;
    (void)fn;
}

/* look up this key of length n in this hash,
 * returning a const pointer to the result if found or NULL otherwise
 */
const struct hash_lookup_result * hash_lookup(
        struct hash * hash,
        const char * key,
        size_t n
    ) [[gnu::nonnull(1, 2)]]
{
    // TODO
    (void)hash;
    (void)key;
    (void)n;
    return NULL;
}

/* fill statistics with statistics on this hash */
void hash_get_statistics(
        struct hash * hash,
        struct hash_statistics * statistics
    ) [[gnu::nonnull(1, 2)]]
{
    // TODO
    (void)hash;
#ifdef HASH_STATISTICS
    *statistics = hash->statistics;
#else
    *statistics = (struct hash_statistics) { };
#endif /* HASH_STATISTICS */
}

/* create a hash_inputs structure, optionally filling it with all the elements
 * that were in from (or no elements if from is NULL)
 */
[[nodiscard]] struct hash_inputs * hash_inputs_create(struct hash * from)
{
    // TODO: the from part
    (void)from;
    struct hash_inputs * hash_inputs = malloc(sizeof(*hash_inputs));
    *hash_inputs = (struct hash_inputs) { };
    return hash_inputs;
}

/* destroy a hash_inputs structure */
void hash_inputs_destroy(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]]
{
    for (size_t i = 0; i < hash_inputs->n_inputs; i++) {
        free(hash_inputs->inputs[i].s);
    }
    free(hash_inputs->inputs);
    free(hash_inputs);
}

/* grow the capacity of hash_inputs by n */
void hash_inputs_grow(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]]
{
#ifdef HASH_STATISTICS
    hash_inputs->statistics.n_growths++;
#endif /* HASH_STASTICS */
    hash_inputs->capacity += n;
    hash_inputs->inputs = realloc(
            hash_inputs->inputs,
            sizeof(*hash_inputs->inputs) * hash_inputs->capacity
        );
}

/* pre-allocate space in hash_inputs for at least n inputs */
void hash_inputs_at_least(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]]
{
    if (hash_inputs->capacity < n) {
        hash_inputs_grow(hash_inputs, n - hash_inputs->capacity);
    }
}

/* add this string of length n to this hash_inputs, associating it with ptr */
void hash_inputs_add(
        struct hash_inputs * hash_inputs,
        const char * s,
        size_t n,
        void * ptr
    ) [[gnu::nonnull(1, 2)]]
{
    assert(hash_inputs->n_inputs <= hash_inputs->capacity);
    if (hash_inputs->n_inputs == hash_inputs->capacity) {
        hash_inputs_grow(hash_inputs, hash_inputs_grow_increment);
    }
    hash_inputs->inputs[hash_inputs->n_inputs] = (struct hash_input) {
        .s = util_strndup(s, n),
        .n = n,
        .ptr = ptr
    };
    hash_inputs->n_inputs++;
}

/* apply this function over every input*/
void hash_inputs_apply(
        struct hash_inputs * hash_inputs,
        void (*fn)(const char * s, size_t n, void ** ptr)
    ) [[gnu::nonnull(1, 2)]]
{
    for (size_t i = 0; i < hash_inputs->n_inputs; i++) {
        struct hash_input * input = &hash_inputs->inputs[i];
        fn(input->s, input->n, &input->ptr);
    }
}

/* fill statistics with statistics on this hash_inputs */
void hash_inputs_get_statistics(
        struct hash_inputs * hash_inputs,
        struct hash_inputs_statistics * statistics
    ) [[gnu::nonnull(1, 2)]]
{
#ifdef HASH_STATISTICS
    hash_inputs->statistics.capacity = hash_inputs->capacity;
    *statistics = hash_inputs->statistics;
#else
    *statistics = (struct hash_inputs_statistics) {
        .capacity = hash_inputs->capacity
    };
#endif /* HASH_STATISTICS */
}

