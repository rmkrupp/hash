/* File: include/hash.h
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

/* this is a hashing library
 *
 * it is meant for cases where you have keys that are (relatively) fixed while
 * matching, i.e. when you can (re-)generate the hash table every time keys
 * change because that doesn't happen frequently.
 *
 * create a struct hash_inputs with hash_inputs_create(), add in the keys you
 * want to hash with hash_inputs_add(), and then turn them into a hash table
 * with hash_create
 */

/*
 * there is a hard limit for number of keys, around 2^39 if ssize_t can hold
 * a 63-bit number and hash_iterations_growth_multiplier is 1024. (See hash.c
 * for this.) The library will crash because it has run out of memory before
 * this point, though.
 *
 * also, if you are really using more than 2^31 keys, update salt generation
 * in hash_function_hash (in hash.c) to get more than an int's worth of random
 * data at a time for those cases.
 */

/* a hash table */
struct hash;

/* a list of inputs to create a hash_table with */
struct hash_inputs;

/* the result of a hash_lookup[() */
struct hash_lookup_result {
    const char * key; /* the key, null terminated */
    size_t length; /* the length of key, not including the terminator */
    void * ptr; /* the pointer passed when the key was added */
};

/* calculate a hash table for all the elements in hash_inputs
 *
 * this can fail. if it does, this function returns null.
 *
 * the tuning parameters in src/hash.c can be adjusted if necessary to change
 * how the parameter-space is searched before giving up.
 *
 * calculation depends on the rand() function for randomness and so affects
 * that state and can be affected by seeding with srand(). it does not call
 * srand().
 *
 * if this returns non-null, the keys will have been removed from hash_inputs.
 * it still needs to be free'd.
 *
 * if you want the inputs back, see hash_recycle_inputs
 */
[[nodiscard]] struct hash * hash_create(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]];

/* destroy this hash table */
void hash_destroy(struct hash * hash) [[gnu::nonnull(1)]];

/* destroy this hash table, but extract the hash_input it was created with
 * first and return them for modification and reuse
 */
struct hash_inputs * hash_recycle_inputs(
        struct hash * hash) [[gnu::nonnull(1)]];

/* apply this function over every key this hash was created with */
void hash_apply(
        struct hash * hash,
        void (*fn)(const char * key, size_t length, void ** ptr)
    ) [[gnu::nonnull(1, 2)]];

/* look up this key of length n in this hash,
 * returning a const pointer to the result if found or NULL otherwise
 */
const struct hash_lookup_result * hash_lookup(
        struct hash * hash,
        const char * key,
        size_t length
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

/* create an empty hash_inputs structure */
[[nodiscard]] struct hash_inputs * hash_inputs_create();

/* destroy a hash_inputs structure */
void hash_inputs_destroy(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]];

/* grow the capacity of hash_inputs by n */
void hash_inputs_grow(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]];

/* pre-allocate space in hash_inputs for at least n inputs */
void hash_inputs_at_least(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]];

/* add this string of the given length to this hash_inputs, associating it with
 * ptr
 *
 * a zero-length string cannot be hashed and will be ignored. A warning will
 * be issued unless HASH_NO_WARNINGS
 *
 * this key must not already be in hash_inputs. if it is, the behavior of a
 * hash table generated from these inputs becomes undefined.
 *
 * see hash_inputs_add_safe(), but hash_inputs is not optimized for this case
 * (it does not sort itself) and so hash_inputs_add() is the preferred method
 * of adding keys. sort/deduplicate them beforehand!
 */
void hash_inputs_add(
        struct hash_inputs * hash_inputs,
        const char * key,
        size_t length,
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

/* see hash_inputs_add
 *
 * this lifts the requirement of key uniqueness by comparing this key to every
 * key already in the table. this will not protect future calls to
 * hash_inputs_add() with this key!
 *
 * use mindfully and with caution
 */
void hash_inputs_add_safe(
        struct hash_inputs * hash_inputs,
        const char * key,
        size_t length,
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

/* apply this function over every input*/
void hash_inputs_apply(
        struct hash_inputs * hash_inputs,
        void (*fn)(const char * key, size_t length, void ** ptr)
    ) [[gnu::nonnull(1, 2)]];

/* the statistics filled by hash_inputs_get_statistics */
struct hash_inputs_statistics {
    size_t n_growths; /* how many times did the pool get grown by:
                       *  - hash_inputs_add[_safe]
                       *  - hash_inputs_grow
                       *  - hash_inputs_at_least
                       */
    size_t capacity; /* the internal capacity of the pool
                      * may be greater than the number of keys if the grow
                      * or at_least functions were used, or if
                      * hash_inputs_grow_increment is more than 1
                      */
    size_t n_safe_adds_were_safe; /* how many times did hash_inputs_add_safe
                                   * get called when the key was not already
                                   * added
                                   */
    size_t n_safe_adds_were_unsafe; /* how many times did hash_inputs_add_safe
                                     * get called when the key WAS already
                                     * added
                                     */
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
