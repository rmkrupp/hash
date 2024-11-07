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

    srand(time(NULL));

    struct hash_inputs * hash_inputs = hash_inputs_create();
    hash_inputs_at_least(hash_inputs, 100000);

    FILE * f = fopen("keys", "r");
    char * buffer = malloc(1024);
    while (fgets(buffer, 1024, f)) {
        size_t n = strlen(buffer);
        buffer[n - 1] = '\0';
        hash_inputs_add(hash_inputs, buffer, n - 1, NULL);
    }
    fclose(f);
    free(buffer);

    struct hash_inputs_statistics hash_inputs_statistics;
    hash_inputs_get_statistics(hash_inputs, &hash_inputs_statistics);
    printf("[instats] n_growths = %zu\n", hash_inputs_statistics.n_growths);
    printf("[instats] capacity = %zu\n", hash_inputs_statistics.capacity);

    struct hash * hash = hash_create(hash_inputs);
    hash_inputs_destroy(hash_inputs);

    if (!hash) {
        printf("hash is null\n");
        return 1;
    }

    struct hash_statistics statistics;
    hash_get_statistics(hash, &statistics);

    printf("key_length_max = %zu\n", statistics.key_length_max);
    printf("iterations = %zu\n", statistics.iterations);
    printf("nodes_explored = %zu\n", statistics.nodes_explored);
    printf("rand_calls = %zu\n", statistics.rand_calls);
    printf("hashes_calculated = %zu\n", statistics.hashes_calculated);
    printf("graph_size = %zu\n", statistics.graph_size);
    printf("vertex_stack_capacity = %zu\n", statistics.vertex_stack_capacity);
    printf("edges_allocated = %zu\n", statistics.edges_allocated);
    printf("edges_preallocated = %zu\n", statistics.edges_preallocated);
    printf("unneeded_edges_allocated = %zu\n",
            statistics.unneeded_edges_allocated);
    printf("edge_capacity_min = %zu\n", statistics.edge_capacity_min);
    printf("edge_capacity_max = %zu\n", statistics.edge_capacity_max);
    printf("net_memory_allocated = %zu\n", statistics.net_memory_allocated);
    printf("total_memory_allocated = %zu\n",
            statistics.total_memory_allocated);
    printf("reallocs_edges = %zu\n", statistics.reallocs_edges);
    printf("reallocs_salt = %zu\n", statistics.reallocs_salt);
    printf("reallocs_stack = %zu\n", statistics.reallocs_stack);
    printf("reallocs_vertices = %zu\n", statistics.reallocs_vertices);

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
