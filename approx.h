/*
 * approx - non-interactive POSIX fuzzy stream filter and ranker
 * See LICENSE file for copyright and license details.
 */

#ifndef APPROX_H__
#define APPROX_H__

#include <stddef.h>

#define VERSION "1.0"
#define DEFAULT_THRESHOLD 0.70

struct match_item {
	double score;
	char *line;
	size_t lineno;
};

struct heap {
	struct match_item *items;
	size_t size;
	size_t cap;
};

/* Similarity functions */
double sim_substr(const char *pat, size_t patlen, const char *line, size_t linelen, int icase);
double sim_exact(const char *pat, size_t patlen, const char *line, size_t linelen, int icase);

/* Priority queue / heap for top-N ranking */
struct heap *heap_create(size_t cap);
void heap_free(struct heap *h);
void heap_push(struct heap *h, double score, const char *line, size_t lineno);
void heap_sort_descending(struct heap *h);

#endif
