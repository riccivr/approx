/*
 * approx - non-interactive POSIX fuzzy stream filter and ranker
 * See LICENSE file for copyright and license details.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>

#include "arg.h"
#include "approx.h"

char *argv0;

static double threshold = DEFAULT_THRESHOLD;
static int opt_score = 0;
static int opt_icase = 0;
static int opt_invert = 0;
static int opt_exact = 0;
static long opt_topn = 0;

static void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(2);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-isveV] [-t threshold] [-n count] pattern [file ...]\n", argv0);
	exit(2);
}

static inline int
min3(int a, int b, int c)
{
	int m = a < b ? a : b;
	return m < c ? m : c;
}

static inline int
char_eq(char a, char b, int icase)
{
	if (icase)
		return tolower((unsigned char)a) == tolower((unsigned char)b);
	return a == b;
}

double
sim_substr(const char *pat, size_t patlen, const char *line, size_t linelen, int icase)
{
	int buf_a[256], buf_b[256];
	int *prev, *curr, *tmp;
	int *alloc_a = NULL, *alloc_b = NULL;
	int min_dist;
	size_t i, j;

	if (patlen == 0)
		return 1.0;
	if (linelen == 0)
		return 0.0;

	if (patlen + 1 <= sizeof(buf_a) / sizeof(buf_a[0])) {
		prev = buf_a;
		curr = buf_b;
	} else {
		alloc_a = malloc((patlen + 1) * sizeof(int));
		alloc_b = malloc((patlen + 1) * sizeof(int));
		if (!alloc_a || !alloc_b) {
			free(alloc_a);
			free(alloc_b);
			die("approx: out of memory");
		}
		prev = alloc_a;
		curr = alloc_b;
	}

	for (i = 0; i <= patlen; i++)
		prev[i] = (int)i;

	min_dist = (int)patlen;

	for (j = 0; j < linelen; j++) {
		char c = line[j];
		curr[0] = 0;

		for (i = 1; i <= patlen; i++) {
			int cost = char_eq(pat[i - 1], c, icase) ? 0 : 1;
			curr[i] = min3(curr[i - 1] + 1,
			               prev[i] + 1,
			               prev[i - 1] + cost);
		}

		if (curr[patlen] < min_dist)
			min_dist = curr[patlen];

		tmp = prev;
		prev = curr;
		curr = tmp;
	}

	if (alloc_a) {
		free(alloc_a);
		free(alloc_b);
	}

	if (min_dist >= (int)patlen)
		return 0.0;

	return 1.0 - ((double)min_dist / (double)patlen);
}

double
sim_exact(const char *pat, size_t patlen, const char *line, size_t linelen, int icase)
{
	int buf_a[256], buf_b[256];
	int *prev, *curr, *tmp;
	int *alloc_a = NULL, *alloc_b = NULL;
	int dist;
	size_t max_len, i, j;

	if (patlen == 0 && linelen == 0)
		return 1.0;
	if (patlen == 0 || linelen == 0)
		return 0.0;

	if (patlen + 1 <= sizeof(buf_a) / sizeof(buf_a[0])) {
		prev = buf_a;
		curr = buf_b;
	} else {
		alloc_a = malloc((patlen + 1) * sizeof(int));
		alloc_b = malloc((patlen + 1) * sizeof(int));
		if (!alloc_a || !alloc_b) {
			free(alloc_a);
			free(alloc_b);
			die("approx: out of memory");
		}
		prev = alloc_a;
		curr = alloc_b;
	}

	for (i = 0; i <= patlen; i++)
		prev[i] = (int)i;

	for (j = 0; j < linelen; j++) {
		char c = line[j];
		curr[0] = (int)(j + 1);

		for (i = 1; i <= patlen; i++) {
			int cost = char_eq(pat[i - 1], c, icase) ? 0 : 1;
			curr[i] = min3(curr[i - 1] + 1,
			               prev[i] + 1,
			               prev[i - 1] + cost);
		}

		tmp = prev;
		prev = curr;
		curr = tmp;
	}

	dist = prev[patlen];

	if (alloc_a) {
		free(alloc_a);
		free(alloc_b);
	}

	max_len = patlen > linelen ? patlen : linelen;
	if (dist >= (int)max_len)
		return 0.0;

	return 1.0 - ((double)dist / (double)max_len);
}

struct heap *
heap_create(size_t cap)
{
	struct heap *h;

	if (cap == 0)
		return NULL;

	h = malloc(sizeof(*h));
	if (!h)
		die("approx: out of memory");

	h->items = calloc(cap, sizeof(h->items[0]));
	if (!h->items) {
		free(h);
		die("approx: out of memory");
	}
	h->size = 0;
	h->cap = cap;
	return h;
}

void
heap_free(struct heap *h)
{
	size_t i;

	if (!h)
		return;

	for (i = 0; i < h->size; i++)
		free(h->items[i].line);

	free(h->items);
	free(h);
}

static void
sift_up(struct heap *h, size_t idx)
{
	while (idx > 0) {
		size_t parent = (idx - 1) / 2;
		if (h->items[idx].score < h->items[parent].score) {
			struct match_item tmp = h->items[idx];
			h->items[idx] = h->items[parent];
			h->items[parent] = tmp;
			idx = parent;
		} else {
			break;
		}
	}
}

static void
sift_down(struct heap *h, size_t idx)
{
	size_t smallest = idx;
	size_t left = 2 * idx + 1;
	size_t right = 2 * idx + 2;

	if (left < h->size && h->items[left].score < h->items[smallest].score)
		smallest = left;
	if (right < h->size && h->items[right].score < h->items[smallest].score)
		smallest = right;

	if (smallest != idx) {
		struct match_item tmp = h->items[idx];
		h->items[idx] = h->items[smallest];
		h->items[smallest] = tmp;
		sift_down(h, smallest);
	}
}

void
heap_push(struct heap *h, double score, const char *line, size_t lineno)
{
	char *dup;

	if (h->size < h->cap) {
		dup = strdup(line);
		if (!dup)
			die("approx: out of memory");
		h->items[h->size].score = score;
		h->items[h->size].line = dup;
		h->items[h->size].lineno = lineno;
		h->size++;
		sift_up(h, h->size - 1);
	} else if (score > h->items[0].score) {
		dup = strdup(line);
		if (!dup)
			die("approx: out of memory");
		free(h->items[0].line);
		h->items[0].score = score;
		h->items[0].line = dup;
		h->items[0].lineno = lineno;
		sift_down(h, 0);
	}
}

static int
cmp_items_desc(const void *a, const void *b)
{
	const struct match_item *ia = a;
	const struct match_item *ib = b;

	if (ia->score < ib->score)
		return 1;
	if (ia->score > ib->score)
		return -1;
	return (ia->lineno > ib->lineno) - (ia->lineno < ib->lineno);
}

void
heap_sort_descending(struct heap *h)
{
	if (!h || h->size == 0)
		return;
	qsort(h->items, h->size, sizeof(h->items[0]), cmp_items_desc);
}

static int
process_stream(FILE *fp, const char *pat, size_t patlen, struct heap *h)
{
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	size_t lineno = 0;
	int matched = 0;

	while ((linelen = getline(&line, &linecap, fp)) >= 0) {
		double score;
		int is_match;
		size_t len = (size_t)linelen;

		lineno++;

		/* Strip trailing newline and CR for similarity calculation */
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		if (opt_exact)
			score = sim_exact(pat, patlen, line, len, opt_icase);
		else
			score = sim_substr(pat, patlen, line, len, opt_icase);

		is_match = opt_invert ? (score < threshold) : (score >= threshold);

		if (h) {
			if (is_match) {
				heap_push(h, score, line, lineno);
				matched = 1;
			}
		} else if (is_match) {
			matched = 1;
			if (opt_score)
				printf("%.2f\t%s\n", score, line);
			else
				puts(line);
		}
	}

	free(line);
	return matched;
}

int
main(int argc, char *argv[])
{
	char *pat;
	size_t patlen;
	struct heap *h = NULL;
	int matched = 0;
	int err = 0;

	ARGBEGIN {
	case 't': {
		char *s = EARGF(usage());
		char *end;
		threshold = strtod(s, &end);
		if (*end != '\0' || threshold < 0.0 || threshold > 1.0)
			die("approx: invalid threshold: %s (must be 0.0 to 1.0)", s);
		break;
	}
	case 'n': {
		char *s = EARGF(usage());
		char *end;
		opt_topn = strtol(s, &end, 10);
		if (*end != '\0' || opt_topn <= 0)
			die("approx: invalid count: %s", s);
		break;
	}
	case 's':
		opt_score = 1;
		break;
	case 'i':
		opt_icase = 1;
		break;
	case 'v':
		opt_invert = 1;
		break;
	case 'e':
		opt_exact = 1;
		break;
	case 'V':
		puts("approx-" VERSION);
		return 0;
	default:
		usage();
	} ARGEND;

	if (argc < 1)
		usage();

	pat = argv[0];
	patlen = strlen(pat);
	argc--;
	argv++;

	if (opt_topn > 0)
		h = heap_create((size_t)opt_topn);

	if (argc == 0) {
		if (process_stream(stdin, pat, patlen, h))
			matched = 1;
	} else {
		int i;
		for (i = 0; i < argc; i++) {
			FILE *fp;
			if (strcmp(argv[i], "-") == 0) {
				fp = stdin;
			} else {
				fp = fopen(argv[i], "r");
				if (!fp) {
					fprintf(stderr, "approx: %s: %s\n", argv[i], strerror(errno));
					err = 2;
					continue;
				}
			}

			if (process_stream(fp, pat, patlen, h))
				matched = 1;

			if (fp != stdin)
				fclose(fp);
		}
	}

	if (h) {
		size_t i;
		heap_sort_descending(h);
		for (i = 0; i < h->size; i++) {
			if (opt_score)
				printf("%.2f\t%s\n", h->items[i].score, h->items[i].line);
			else
				puts(h->items[i].line);
		}
		heap_free(h);
	}

	if (err)
		return err;

	return matched ? 0 : 1;
}
