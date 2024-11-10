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
 *
 * keys are always given with a length and may contain any value, including
 * embedded null bytes. the const char * key pointers handed out as part of a
 * lookup result or apply function will always include a null byte after the
 * key, i.e. at key[length]. thus, if passing strings as keys, their length
 * need not include the null terminator.
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

/* a list of keys to create a hash_table with */
struct hash_inputs;

/* the result of a hash_lookup() */
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
 * if you want the keys back, see hash_recycle_inputs, hash_get_keys, and
 * hash_inputs_from_hash.
 */
[[nodiscard]] struct hash * hash_create(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]];

/* destroy this hash table */
void hash_destroy(struct hash * hash) [[gnu::nonnull(1)]];

/* returns the number of keys in this hash */
size_t hash_n_keys(const struct hash * hash) [[gnu::nonnull(1)]];

/* destroy this hash table, but extract the hash_inputs it was created with
 * first and return it for modification and reuse
 *
 * this is a new hash_inputs and needs to be free'd separely from the one
 * passed to hash_create. moreover, free'ing that hash_inputs has no effect
 * on the use or results of this function
 */
struct hash_inputs * hash_recycle_inputs(
        struct hash * hash) [[gnu::nonnull(1)]];

/* returns a pointer to the keys inside this hash table and, if n_keys_out
 * is non-NULL, sets it to the number of keys
 */
const struct hash_lookup_result * hash_get_keys(
        const struct hash * hash, size_t * n_keys_out) [[gnu::nonnull(1)]];

/* apply this function over every key this hash was created with */
void hash_apply(
        const struct hash * hash,
        void (*fn)(
            const char * key, size_t length, void * data, void * ptr),
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

/* look up this key of length n in this hash and return const pointer to the
 * result if found or NULL otherwise
 */
const struct hash_lookup_result * hash_lookup(
        const struct hash * hash,
        const char * key,
        size_t length
    ) [[gnu::nonnull(1, 2)]];

/* the statistics filled by hash_get_statistics */
struct hash_statistics {
    size_t key_length_max; /* the length of the longest key */
    size_t iterations; /* number of iterations */
    size_t nodes_explored; /* number of vertices marked visited on across all
                            * iterations
                            */
    size_t rand_calls; /* number of calls to rand() by the hash function */
    size_t hashes_calculated; /* number of calls to the hash function */
    size_t graph_size; /* size of the graph / "values" table */
    size_t vertex_stack_capacity; /* amount of slots allocated for the vertex
                                   * stack
                                   */
    size_t edges_allocated; /* number of edges allocated by realloc, i.e. not
                             * including those pre-allocated
                             */
    size_t edges_preallocated; /* number of edges allocated by preallocation */
    size_t unneeded_edges_allocated; /* number of edges allocated on vertices
                                      * where simply growing wouldn't have
                                      * allocated them
                                      */
    size_t edge_capacity_min; /* the number of edge slots allocated on the
                               * vertex where that number is smallest
                               */
    size_t edge_capacity_max; /* the number of edge slots allocated on the
                               * vertex where that number is largest
                               */
    size_t net_memory_allocated; /* amount of memory allocated, counting only
                                  * the additional memory of reallocs, not
                                  * the whole amount
                                  */
    size_t total_memory_allocated; /* amount of memory, counting each realloc
                                    * as a separate allocation
                                    */
    size_t reallocs_edges; /* number of times an edge list was realloc'd */
    size_t reallocs_salt; /* number of times salt was realloc'd */
    size_t reallocs_stack; /* number of times the vertax stack was realloc'd */
    size_t reallocs_vertices; /* number of times the vertex list was
                               * realloc'd
                               */
    size_t realloc_amount_edges; /* amount of memory returned by realloc when
                                  * realloc'ing edges
                                  */
    size_t realloc_amount_salt; /* amount of memory returned by realloc when
                                 * realloc'ing salt
                                 */
    size_t realloc_amount_stack; /* amount of memory returned by realloc when
                                 * realloc'ing the vertex stack
                                 */
    size_t realloc_amount_vertices; /* amount of memory returned by realloc
                                     * when realloc'ing vertices
                                     */
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

/* create a hash_inputs structure containing all the keys in this hash
 *
 * note that if you are done with the hash, hash_recycles_inputs is more
 * efficient because it recycles the keys in place.
 */
[[nodiscard]] struct hash_inputs * hash_inputs_from_hash(
        struct hash * hash) [[gnu::nonnull(1)]];

/* destroy a hash_inputs structure */
void hash_inputs_destroy(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]];

/* returns the number of keys in this hash_inputs */
size_t hash_inputs_n_keys(
        const struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]];

/* destroy a hash_inputs structure without free'ing its keys
 *
 * use this in conjunction with hash_inputs_add_no_copy()
 *
 * if hash_inputs_add() or hash_inputs_add_safe() have been called on this
 * hash_inputs (or if they were called on any of its parents and
 * hash_reycle_inputs() is involved), calling this function will leak memory.
 */
void hash_inputs_destroy_except_keys(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]];

/* grow the capacity of hash_inputs by n
 *
 * this affects how many items can be added to n before it has to realloc
 * its internal memory
 */
void hash_inputs_grow(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]];

/* pre-allocate space in hash_inputs for at least n keys */
void hash_inputs_at_least(
        struct hash_inputs * hash_inputs, size_t n) [[gnu::nonnull(1)]];

/* add this key of the given length to this hash_inputs, associating it with
 * ptr
 *
 * a zero-length key cannot be hashed and will be ignored. A warning will
 * be issued unless HASH_NO_WARNINGS
 *
 * this key must not already be in hash_inputs. if it is, the behavior of a
 * hash table generated from these keys becomes undefined.
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

/* see hash_inputs_add
 *
 * this does not make a copy of key and so the key passed must not be free'd
 * except by a call to hash_inputs_destroy() (or, eventually, hash_destroy.)
 *
 * notably, this also does not guarantee that the key will be null-terminated.
 *
 * if you don't want hash_inputs_destroy() to destroy them, use
 * hash_inputs_destroy_except_keys()
 *
 * if you don't want hash_destroy() to destroy them, use hash_recycle_inputs().
 */
void hash_inputs_add_no_copy(
        struct hash_inputs * hash_inputs,
        char * key,
        size_t length,
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

/* apply this function over every key */
void hash_inputs_apply(
        const struct hash_inputs * hash_inputs,
        void (*fn)(
            const char * key, size_t length, void * data, void * ptr),
        void * ptr
    ) [[gnu::nonnull(1, 2)]];

/* apply this function over every key and then destroy the hash inputs
 * (without free'ing the keys, since this is designed to let them escape via
 * the apply)
 */
void hash_inputs_apply_and_destroy(
        struct hash_inputs * hash_inputs,
        void (*fn)(
            char * key, size_t length, void * data, void * ptr),
        void * ptr
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
