/*
 * embed_demo.c - Demonstrates embedding approx.h as an stb-style library
 *
 * Compile:
 *   cc -O2 -I.. embed_demo.c -o embed_demo -lm
 */

#define APPROX_IMPLEMENTATION
#include "approx.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *pattern = "connection timeout";
    const char *logs[] = {
        "2026-08-31 [INFO] client connected",
        "2026-08-31 [ERROR] connectoin timeout on redis",
        "2026-08-31 [WARN] slow query execution (450ms)",
        "2026-08-31 [FATAL] connection timed out after 30s",
        "2026-08-31 [INFO] routine health check ok"
    };
    size_t num_logs = sizeof(logs) / sizeof(logs[0]);
    size_t i;

    printf("=== Basic Substring Similarity ===\n");
    for (i = 0; i < num_logs; i++) {
        size_t start = 0, end = 0;
        double score = approx_sim_span(pattern, strlen(pattern),
                                       logs[i], strlen(logs[i]),
                                       APPROX_ICASE | APPROX_DAMERAU,
                                       &start, &end);
        if (score >= 0.70) {
            printf("Score: %.2f | Span [%zu..%zu] | %s\n", score, start, end, logs[i]);
        }
    }

    printf("\n=== Top-2 Fuzzy Heap Ranking ===\n");
    approx_heap_t *heap = approx_heap_create(2);
    for (i = 0; i < num_logs; i++) {
        double score = approx_sim(pattern, strlen(pattern), logs[i], strlen(logs[i]), APPROX_ICASE);
        approx_heap_push(heap, score, logs[i], strlen(logs[i]), NULL, 0, 0, 0, i);
    }

    approx_heap_sort(heap);
    for (i = 0; i < heap->size; i++) {
        printf("#%zu: score=%.2f -> %s\n", i + 1, heap->items[i].score, heap->items[i].line);
    }
    approx_heap_free(heap);

    return 0;
}
