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

/*
 * This is an implementation of the algorithm laid out in "An optimal algorithm
 * for generating minimal perfect hash functions" by Czech, Havas, and
 * Majewski
 *
 * This code is not derived from the CMPH code found at <cmph.sourceforge.net>
 * but you can find the paper there (at cmph.sourceforge.net/papers/chm92.pdf)
 */

#include "util/strdup.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <assert.h>

#if !defined(HASH_NO_WARNINGS) || !HASH_NO_WARNINGS
#include <stdio.h>
#endif /* HASH_NO_WARNINGS */

/*
 * TODO for version 1.0:
 *  - should there be a grow increment for edges? (multiplicative realloc?)
 *      (at least optionally?)
 *  - Test de-pointerifying the hash functions
 *  - should we do unsigned chars? does the hash handle embedded zero bytes
 *    fine everywhere? (look for strdup/strndup and change)
 *  - go over documentation one more time
 *  - check TODOs
 */

/*
 * TUNING VALUES
 */

#ifndef HASH_PREALLOC_EDGES
#define HASH_PREALLOC_EDGES 0
#endif /* HASH_PREALLOC_EDGES */

/* when adding hash inputs when space hasn't been preallocated by calls to
 * hash_inputs_grow and hash_inputs_at_least
 */
constexpr size_t hash_inputs_grow_increment = 1;

/* hash_create gives up after a number of iterations equal to this multiplier
 * multiplied by the number of keys
 */
constexpr size_t hash_iterations_max_multiplier = 100;

/* hash_create increases the size of the graph after this number of iterations
 * (see the multiplier, though)
 */
constexpr size_t hash_iterations_grow_every_n_trials = 5;

/* internally, hash_create keeps an graph size counter scaled by the divider,
 * and increases it by the formula:
 *   next = (current * multiplier) / divider
 * and derives the actual size by dividing it by the divider again and throwing
 * out the remainder.
 */
constexpr size_t hash_iterations_growth_multiplier = 1075;
constexpr size_t hash_iterations_growth_multiplier_divider = 1024;

/* this is a tuning value that causes some number of edges to be preallocated
 * for every vertex. generally, the number needed (for, say, 100k keys) is
 * between 0 and 12 per vertex. set it somewhere between this, causing the
 * hash to use more memory in exchange for calling realloc less.
 *
 * note: testing has not shown this to be a good idea, at least on Linux
 */
[[maybe_unused]] constexpr size_t hash_prealloc_edges = HASH_PREALLOC_EDGES;

/*
 * TYPES
 */

typedef ssize_t hash_function_result;

/* the state describing a hash function
 *
 * salt is ints because rand() makes ints
 */
struct hash_function {
    size_t * salt;
    size_t salt_length;
    size_t salt_capacity;
    size_t n;
};

/* an individual key
 *
 * (like struct hash_lookup_result but key isn't const)
 */
struct hash_input {
    char * key;
    size_t length;
    void * ptr;
};

/* inputs to create a hash table with */
struct hash_inputs {
    struct hash_input * inputs;
    size_t n_inputs;
    size_t capacity;
#ifdef HASH_STATISTICS
    struct hash_inputs_statistics statistics;
#endif /* HASH_STATISTICS */
};

/* a hash table */
struct hash {
    struct hash_inputs keys;
    struct hash_function f1,
                         f2;
    size_t * values;
    size_t n_values;
#ifdef HASH_STATISTICS
    struct hash_statistics statistics;
#endif /* HASH_STATISTICS */
};

/*
 * GRAPH
 */

/* a graph
 *
 * because n_vertices only ever goes up, there's no need for a separate
 * capacity value
 */
struct graph {
    struct vertex * vertices;
    size_t n_vertices;

    struct vertex_stack_node * vertex_stack;
    size_t vertex_stack_capacity;

#ifdef HASH_STATISTICS
    struct hash_statistics statistics;
#endif /* HASH_STATISTICS */
};

/* a vertex in the above graph */
struct vertex {
    hash_function_result value;
    bool visited;
    struct edge * edges;
    size_t n_edges;
    size_t edge_capacity;
#ifdef HASH_STATISTICS
    size_t n_edges_max;
#endif /* HASH_STATISTICS */
};

#ifdef HASH_STATISTICS
constexpr size_t graph_vertex_statistics_extra =
    sizeof(((struct vertex *)NULL)->n_edges_max);
#endif /* HASH_STATISTICS */

/* an edge in the above graph */
struct edge {
    struct vertex * to;
    hash_function_result value;
};

/* a (vertex, parent) pair for the vertext_stack */
struct vertex_stack_node {
    struct vertex * vertex, * parent;
};

/* create a new, empty graph */
[[nodiscard]] static struct graph * graph_create()
{
    struct graph * graph = malloc(sizeof(*graph));
    *graph = (struct graph) {
        /* pre-allocate space for a single vertex on the stack */
        .vertex_stack = malloc(sizeof(*graph->vertex_stack)),
        .vertex_stack_capacity = 1
    };
#ifdef HASH_STATISTICS
    graph->statistics.total_memory_allocated += sizeof(*graph->vertex_stack);
    graph->statistics.net_memory_allocated += sizeof(*graph->vertex_stack);
#endif /* HASH_STATISTICS */
    return graph;
}

static void graph_destroy(struct graph * graph) [[gnu::nonnull(1)]]
{
    for (size_t i = 0; i < graph->n_vertices; i++) {
        free(graph->vertices[i].edges);
    }
    free(graph->vertices);
    free(graph->vertex_stack);
    free(graph);
}

/* expand the size of this graph to at least n_vertices, and clear the newly
 * allocated vertices. this does not modify pre-existing vertices except by
 * copying them to newly-allocated memory via realloc.
 *
 * note that you still need to call graph_wipe to set the .value property of
 * the each vertex to its proper rest state of -1.
 */
static void graph_at_least(
        struct graph * graph, size_t n_vertices) [[gnu::nonnull(1)]]
{
    assert(n_vertices >= graph->n_vertices);

#ifdef HASH_STATISTICS
    graph->statistics.reallocs_vertices++;
    graph->statistics.realloc_amount_vertices +=
        (sizeof(*graph->vertices) - graph_vertex_statistics_extra)
         * graph->n_vertices;
    graph->statistics.net_memory_allocated +=
        (sizeof(*graph->vertices) - graph_vertex_statistics_extra)
         * n_vertices -
        (sizeof(*graph->vertices) - graph_vertex_statistics_extra)
         * graph->n_vertices;
    graph->statistics.total_memory_allocated +=
        (sizeof(*graph->vertices) - graph_vertex_statistics_extra)
        * n_vertices;
#endif /* HASH_STATISTICS */

    graph->vertices = realloc(
            graph->vertices, sizeof(*graph->vertices) * n_vertices);

    for (size_t i = graph->n_vertices; i < n_vertices; i++) {
        graph->vertices[i] = (struct vertex) {
            /* pre-allocating edges here reduces the number of reallocs,
             * which are otherwise quite high
             * (even if only pre-allocating 4 to 12 edges per vertex),
             * at the expense of using more memory, but ultimately this wasn't
             * a big immediate performance gain. needs more testing.
             *
             * make it a tuneable
             */
            .edges = hash_prealloc_edges ?
                malloc(sizeof(*graph->vertices[i].edges)
                        * hash_prealloc_edges) : NULL,
            .edge_capacity = hash_prealloc_edges
        };
#ifdef HASH_STATISTICS
        graph->statistics.edges_preallocated += hash_prealloc_edges;
        graph->statistics.net_memory_allocated +=
            sizeof(*graph->vertices[i].edges) * hash_prealloc_edges;
        graph->statistics.total_memory_allocated +=
            sizeof(*graph->vertices[i].edges) * hash_prealloc_edges;
#endif /* HASH_STATISTICS */
    }

    graph->n_vertices = n_vertices;
}

/* reset the graph, but keep the same number of allocated vertices and the
 * edge_capacity of each vertex. value is reset for each vertex to -1.
 */
static void graph_wipe(struct graph * graph) [[gnu::nonnull(1)]]
{
    for (size_t i = 0; i < graph->n_vertices; i++) {
        graph->vertices[i] = (struct vertex) {
            .value = -1,
            .edges = graph->vertices[i].edges,
#ifdef HASH_STATISTICS
            .n_edges_max = graph->vertices[i].n_edges_max,
#endif /* HASH_STATISTICS */
            .edge_capacity = graph->vertices[i].edge_capacity
        };
    }
}

/* create an edge from the vertex at from_index to the vertex at to_index
 * with this value.
 *
 * expands the edge pool if necessary.
 */
static void graph_connect(
        struct graph * graph,
        size_t from_index,
        size_t to_index,
        hash_function_result edge_value
    ) [[gnu::nonnull(1)]]
{
    assert(from_index < graph->n_vertices);
    assert(to_index < graph->n_vertices);

    struct vertex * from = &graph->vertices[from_index];

    assert(from->n_edges <= from->edge_capacity);
    if (from->n_edges == from->edge_capacity) {

#ifdef HASH_STATISTICS
        graph->statistics.reallocs_edges++;
        graph->statistics.edges_allocated++;
        graph->statistics.realloc_amount_edges +=
            sizeof(*from->edges) * from->edge_capacity;
        graph->statistics.net_memory_allocated +=
            sizeof(*from->edges) * (from->edge_capacity + 1) -
            sizeof(*from->edges) * from->edge_capacity;
        graph->statistics.total_memory_allocated +=
            sizeof(*from->edges) * (from->edge_capacity + 1);
#endif /* HASH_STATISTICS */

        // TODO grow rate?
        from->edges = realloc(
                from->edges, sizeof(*from->edges) * (from->edge_capacity + 1));
        from->edge_capacity += 1;
    }

#if HASH_PREALLOC_EDGES > 0
    assert(from->edge_capacity >= hash_prealloc_edges);
#endif /* HASH_PREALLOC_EDGES */

    from->edges[from->n_edges] = (struct edge) {
        .to = &graph->vertices[to_index],
        .value = edge_value
    };
    from->n_edges++;

#ifdef HASH_STATISTICS
    if (from->n_edges > from->n_edges_max) {
        from->n_edges_max = from->n_edges;
    }
#endif /* HASH_STATISTICS */
}

/* create two edges, one each way, between the vertex at from_index and the
 * vertex at to_index, and give both this value
 */
static void graph_biconnect(
        struct graph * graph,
        size_t from_index,
        size_t to_index,
        hash_function_result edge_value
    ) [[gnu::nonnull(1)]]
{
    graph_connect(graph, from_index, to_index, edge_value);
    graph_connect(graph, to_index, from_index, edge_value);
}

/* resolve the graph, testing if it is acyclic and generating the appropriate
 * value of each vertex.
 *
 * returns true if acyclic, false otherwise
 */
static bool graph_resolve(struct graph * graph)
{
    struct vertex_stack_node * vertex_stack = graph->vertex_stack;
    size_t vertex_stack_length;
    size_t vertex_stack_capacity = graph->vertex_stack_capacity;

    for (size_t i = 0; i < graph->n_vertices; i++) {
        struct vertex * root = &graph->vertices[i];

        if (root->visited) {
            continue;
        }

        root->value = 0;

        vertex_stack[0] = (struct vertex_stack_node) {
            .vertex = root,
            .parent = NULL
        };
        vertex_stack_length = 1;

        while (vertex_stack_length) {
            struct vertex * vertex =
                vertex_stack[vertex_stack_length - 1].vertex;
            struct vertex * parent =
                vertex_stack[vertex_stack_length - 1].parent;
            vertex_stack_length--;

            vertex->visited = true;

#ifdef HASH_STATISTICS
            graph->statistics.nodes_explored++;
#endif /* HASH_STATISTICS */

            bool skip = true;
            for (size_t j = 0; j < vertex->n_edges; j++) {
                struct edge * edge = &vertex->edges[j];
                struct vertex * to = edge->to;

                if (skip && to == parent) {
                    skip = false;
                    continue;
                }

                if (to->visited) {
                    // cyclic
                    return false;
                }

                assert(vertex_stack_length <= vertex_stack_capacity);
                if (vertex_stack_length == vertex_stack_capacity) {

#ifdef HASH_STATISTICS
                    graph->statistics.reallocs_stack++;
                    graph->statistics.realloc_amount_stack +=
                        sizeof(*vertex_stack) * vertex_stack_capacity;
                    graph->statistics.net_memory_allocated +=
                        sizeof(*vertex_stack) * (vertex_stack_capacity + 1) -
                        sizeof(*vertex_stack) * vertex_stack_capacity;
                    graph->statistics.total_memory_allocated +=
                        sizeof(*vertex_stack) * (vertex_stack_capacity + 1);
#endif /* HASH_STATISTICS */

                    vertex_stack = realloc(
                            vertex_stack,
                            sizeof(*vertex_stack) * (vertex_stack_capacity + 1)
                        );
                    vertex_stack_capacity += 1;
                    graph->vertex_stack = vertex_stack;
                    graph->vertex_stack_capacity = vertex_stack_capacity;
                }
                vertex_stack[vertex_stack_length] =
                    (struct vertex_stack_node) {
                        .vertex = to,
                        .parent = vertex
                    };
                vertex_stack_length++;

                assert((size_t)(hash_function_result)graph->n_vertices ==
                        graph->n_vertices);
                hash_function_result v =
                    (edge->value - vertex->value) %
                    (hash_function_result)graph->n_vertices;

                if (v < 0) {
                    v += (hash_function_result)graph->n_vertices;
                }

                to->value = v;
            }
        }
    }

    for (size_t i = 0; i < graph->n_vertices; i++) {
        assert(graph->vertices[i].value >= 0);
    }

    // acyclic, all vertex values no longer -1
    return true;
}

/*
 * HASH FUNCTIONS
 */

/* reset this hash function (keeping its buffer and capacity but resetting
 * length and setting a new n)
 */
static void hash_function_reset(
        struct hash_function * hash_function, size_t n) [[gnu::nonnull(1)]]
{
    hash_function->salt_length = 0;
    hash_function->n = n;
}

/* apply this hash function to this key of length
 *
 * calls rand() if more salt is needed
 */
static hash_function_result hash_function_hash(
        struct hash_function * hash_function,
        const char * key,
        size_t length
    ) [[gnu::nonnull(1)]]
{
    if (hash_function->salt_length < length) {
        if (hash_function->salt_capacity < length) {
            hash_function->salt = realloc(
                    hash_function->salt,
                    sizeof(*hash_function->salt) * length
                );
            hash_function->salt_capacity = length;
        }

        for (size_t i = hash_function->salt_length; i < length; i++) {
            hash_function->salt[i] = rand() % hash_function->n;
        }
        hash_function->salt_length = length;
    }

    hash_function_result sum = 0;
    for (size_t i = 0; i < length; i++) {
        hash_function_result x =
            (hash_function_result)(unsigned char)key[i] * 
            (hash_function_result)hash_function->salt[i];
        assert(x >= 0);
        assert((sum + x) >= sum);
        sum += x;
    }

    assert(sum >= 0);

    sum = sum % hash_function->n;

    return sum;
}

/*
 * THE HASH TABLE
 */

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
 * if this returns non-null, consider the hash_inputs not-for-use going
 * forward. notably, it should not be destroyed or free'd except by a call to
 * hash_destroy() on the returned table. if you want it back, and are done
 * with the table, call hash_recycle_inputs().
 *
 * adding keys to a hash_inputs that has already been passed to a table will
 * not, currently, break it, and hash_inputs_add() does not keep the string
 * pointer added to it (it makes its own copy) so changing it after is fine.
 *
 * if you do manage to change it, the hash may work unexpectedly, including
 * failing to match, because all lookups are validated by comparing against
 * the location where the string *should* be in the internal reference to
 * hash_inputs the hash table contains.
 */
[[nodiscard]] struct hash * hash_create(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]]
{
    size_t n_keys = hash_inputs->n_inputs;

    if (n_keys == 0) {
        return NULL;
    }

    size_t n_vertices = n_keys + 1;
    size_t n_vertices_scaled = n_vertices *
        hash_iterations_growth_multiplier_divider;

    struct graph * graph = graph_create();
    graph_at_least(graph, n_vertices);

#ifdef HASH_STATISTICS
    graph->statistics.graph_size = n_vertices;
    graph->statistics.total_memory_allocated +=
        sizeof(*graph) - sizeof(struct hash_statistics);
    graph->statistics.net_memory_allocated +=
        sizeof(*graph) - sizeof(struct hash_statistics);
#endif /* HASH_STATISTICS */

    //struct hash_function f1, f2;
    //f1 = (struct hash_function) { };
    //f2 = (struct hash_function) { };
    struct hash_function * f1 = malloc(sizeof(*f1));
    struct hash_function * f2 = malloc(sizeof(*f2));
    *f1 = (struct hash_function) { };
    *f2 = (struct hash_function) { };

    size_t iteration = 0;
    size_t iteration_max = hash_iterations_max_multiplier * n_vertices;

    do {
        if (iteration % hash_iterations_grow_every_n_trials == 0) {
            if (iteration > 0) {
                if (iteration >= iteration_max) {
#if !defined(HASH_NO_WARNINGS) || !HASH_NO_WARNINGS
                    fprintf(
                            stderr,
                            "WARNING: hash_create() ran for more than size * hash_iteration_max_multiplier iterations (%lu) without a solution\n",
                            iteration
                        );
#endif /* HASH_NO_WARNINGS */
                    free(f1->salt);
                    free(f2->salt);
                    free(f1);
                    free(f2);
                    graph_destroy(graph);
                    return NULL;
                }

                // time to grow the size of the graph
                n_vertices_scaled *= hash_iterations_growth_multiplier;
                n_vertices_scaled /= hash_iterations_growth_multiplier_divider;

                size_t n_vertices_next = 
                    n_vertices_scaled /
                    hash_iterations_growth_multiplier_divider;

                if (n_vertices_next > n_vertices) {
                    n_vertices = n_vertices_next;
                }

#ifdef HASH_STATISTICS
                graph->statistics.graph_size = n_vertices;
#endif /* HASH_STATISTICS */

                graph_at_least(graph, n_vertices);
            }
        }

        assert(iteration + 1 > iteration);

#ifdef HASH_STATISTICS
        graph->statistics.iterations++;
#endif /* HASH_STATISTICS */

        iteration++;

        graph_wipe(graph);

        hash_function_reset(f1, n_vertices);
        hash_function_reset(f2, n_vertices);
        
        for (size_t i = 0; i < n_keys; i++) {
            const char * key = hash_inputs->inputs[i].key;
            size_t length = hash_inputs->inputs[i].length;

#ifdef HASH_STATISTICS
            if (length > f1->salt_capacity) {
                graph->statistics.reallocs_salt++;
                graph->statistics.realloc_amount_salt +=
                    sizeof(*f1->salt) * length;
                graph->statistics.net_memory_allocated +=
                    sizeof(*f1->salt) * length -
                    sizeof(*f1->salt) * f1->salt_capacity;
                graph->statistics.total_memory_allocated +=
                    sizeof(*f1->salt) * length;
                graph->statistics.rand_calls += (length - f1->salt_capacity);
            }
            if (length > f2->salt_capacity) {
                graph->statistics.reallocs_salt++;
                graph->statistics.realloc_amount_salt +=
                    sizeof(*f2->salt) * length;
                graph->statistics.net_memory_allocated +=
                    sizeof(*f2->salt) * length -
                    sizeof(*f2->salt) * f2->salt_capacity;
                graph->statistics.total_memory_allocated +=
                    sizeof(*f2->salt) * length;
                graph->statistics.rand_calls += (length - f2->salt_capacity);
            }
            graph->statistics.hashes_calculated += 2;
#endif /* HASH_STATISTICS */

            hash_function_result r1 = hash_function_hash(f1, key, length);
            hash_function_result r2 = hash_function_hash(f2, key, length);

            graph_biconnect(graph, r1, r2, i);
        }
    } while (!graph_resolve(graph));

#ifndef NDEBUG
    for (size_t i = 0; i < n_keys; i++) {
        const char * key = hash_inputs->inputs[i].key;
        size_t length = hash_inputs->inputs[i].length;
        hash_function_result r1 = hash_function_hash(f1, key, length);
        hash_function_result r2 = hash_function_hash(f2, key, length);
        hash_function_result v1 = graph->vertices[r1].value;
        hash_function_result v2 = graph->vertices[r2].value;
        hash_function_result v = (v1 + v2) % graph->n_vertices;
        if (v < 0) {
            v += graph->n_vertices;
        }
        assert((ssize_t)i == v);
    }
#endif /* NDEBUG */

#ifdef HASH_STATISTICS
    size_t min_capacity = graph->vertices[0].edge_capacity;
    size_t max_capacity = graph->vertices[0].edge_capacity;
    size_t unneeded = 0;
    for (size_t i = 0; i < graph->n_vertices; i++) {
        if (graph->vertices[i].edge_capacity < min_capacity) {
            min_capacity = graph->vertices[i].edge_capacity;
        }
        if (graph->vertices[i].edge_capacity > max_capacity) {
            max_capacity = graph->vertices[i].edge_capacity;
        }
        assert(graph->vertices[i].n_edges <= graph->vertices[i].edge_capacity);
        unneeded +=
            graph->vertices[i].edge_capacity - graph->vertices[i].n_edges_max;
    }
    graph->statistics.edge_capacity_min = min_capacity;
    graph->statistics.edge_capacity_max = max_capacity;
    graph->statistics.unneeded_edges_allocated = unneeded;

    graph->statistics.vertex_stack_capacity = graph->vertex_stack_capacity;

    assert(f1->salt_length == f2->salt_length);
    graph->statistics.key_length_max = f1->salt_length;
    graph->statistics.key_length_max = f1->salt_capacity;
#endif /* HASH_STATISTICS */

    /*
     * TODO: we could restructure graph so that it keeps all the values in an
     *       array we could extract here instead of split up over the
     *       vertices
     */
    struct hash * hash = malloc(sizeof(*hash));
    *hash = (struct hash) {
        .keys = *hash_inputs,
        .f1 = *f1,
        .f2 = *f2,
        .values = malloc(sizeof(*hash->values) * graph->n_vertices),
        .n_values = graph->n_vertices
    };

    free(f1);
    free(f2);

#ifdef HASH_STATISTICS
    hash->statistics = graph->statistics;
#endif /* HASH_STATISTICS */

    *hash_inputs = (struct hash_inputs) { };

    for (size_t i = 0; i < graph->n_vertices; i++) {
        hash->values[i] = graph->vertices[i].value;
    }

    graph_destroy(graph);

    return hash;
}

/* destroy this hash table */
void hash_destroy(struct hash * hash) [[gnu::nonnull(1)]]
{
    free(hash->f1.salt);
    free(hash->f2.salt);
    if (hash->keys.inputs) {
        for (size_t i = 0; i < hash->keys.n_inputs; i++) {
            free(hash->keys.inputs[i].key);
        }
        free(hash->keys.inputs);
    }
    free(hash->values);
    free(hash);
}

/* destroy this hash table, but extract the keys it was created with
 * first and return them in a fresh hash_inputs struct
 */
struct hash_inputs * hash_recycle_inputs(
        struct hash * hash) [[gnu::nonnull(1)]]
{
    struct hash_inputs * inputs = hash_inputs_create();
    *inputs = hash->keys;
    hash->keys.inputs = NULL;
    hash_destroy(hash);
    return inputs;
}

/* apply this function over every element of this hash */
void hash_apply(
        struct hash * hash,
        void (*fn)(const char * s, size_t key, void ** ptr)
    ) [[gnu::nonnull(1, 2)]]
{
    hash_inputs_apply(&hash->keys, fn);
}

/* look up this key of length n in this hash,
 * returning a const pointer to the result if found or NULL otherwise
 */
const struct hash_lookup_result * hash_lookup(
        struct hash * hash,
        const char * key,
        size_t length
    ) [[gnu::nonnull(1, 2)]]
{
    assert(hash->f1.n == hash->n_values);
    assert(hash->f2.n == hash->n_values);

    if (length > hash->f1.salt_length || length > hash->f2.salt_length) {
        return NULL;
    }

    hash_function_result r1 = hash_function_hash(&hash->f1, key, length);
    hash_function_result r2 = hash_function_hash(&hash->f2, key, length);
    hash_function_result i = hash->values[r1] + hash->values[r2];

    assert(i >= 0);
    i = i % hash->n_values;

    if ((size_t)i >= hash->keys.n_inputs) {
        return NULL;
    }

    struct hash_input * input = &hash->keys.inputs[i];

    if (input->length != length) {
        return NULL;
    }

    if (strcmp(input->key, key) != 0) {
        return NULL;
    }

    return (const struct hash_lookup_result *)input;
}

/* fill statistics with statistics on this hash */
void hash_get_statistics(
        struct hash * hash,
        struct hash_statistics * statistics
    ) [[gnu::nonnull(1, 2)]]
{
#ifdef HASH_STATISTICS
    *statistics = hash->statistics;
#else
    (void)hash;
    *statistics = (struct hash_statistics) { };
#endif /* HASH_STATISTICS */
}

/* create an empty hash_inputs structure */
[[nodiscard]] struct hash_inputs * hash_inputs_create()
{
    struct hash_inputs * hash_inputs = malloc(sizeof(*hash_inputs));
    *hash_inputs = (struct hash_inputs) { };
    return hash_inputs;
}

/* destroy a hash_inputs structure */
void hash_inputs_destroy(
        struct hash_inputs * hash_inputs) [[gnu::nonnull(1)]]
{
    for (size_t i = 0; i < hash_inputs->n_inputs; i++) {
        free(hash_inputs->inputs[i].key);
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
    ) [[gnu::nonnull(1, 2)]]
{
    if (!length) {
#if !defined(HASH_NO_WARNINGS) && !HASH_NO_WARNINGS
        fprintf(
                stderr,
                "WARNING: hash_inputs_add() was called with a zero-length string\n"
            );
#endif /* HASH_NO_WARNINGS */
        return;
    }

    assert(hash_inputs->n_inputs <= hash_inputs->capacity);
    if (hash_inputs->n_inputs == hash_inputs->capacity) {
        hash_inputs_grow(hash_inputs, hash_inputs_grow_increment);
    }
    hash_inputs->inputs[hash_inputs->n_inputs] = (struct hash_input) {
        .key = util_strndup(key, length),
        .length = length,
        .ptr = ptr
    };
    hash_inputs->n_inputs++;
}

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
    ) [[gnu::nonnull(1, 2)]]
{
    for (size_t i = 0; i < hash_inputs->n_inputs; i++) {
        if (strcmp(hash_inputs->inputs[i].key, key) == 0) {
#ifdef HASH_STATISTICS
            hash_inputs->statistics.n_safe_adds_were_unsafe++;
#endif /* HASH_STATISTICS */
            return;
        }
    }
#ifdef HASH_STATISTICS
    hash_inputs->statistics.n_safe_adds_were_safe++;
#endif /* HASH_STATISTICS */
    hash_inputs_add(hash_inputs, key, length, ptr);
}

/* apply this function over every input*/
void hash_inputs_apply(
        struct hash_inputs * hash_inputs,
        void (*fn)(const char * key, size_t length, void ** ptr)
    ) [[gnu::nonnull(1, 2)]]
{
    for (size_t i = 0; i < hash_inputs->n_inputs; i++) {
        struct hash_input * input = &hash_inputs->inputs[i];
        fn(input->key, input->length, &input->ptr);
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
    (void)hash_inputs;
    *statistics = (struct hash_inputs_statistics) { };
#endif /* HASH_STATISTICS */
}

