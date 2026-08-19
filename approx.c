/*
 * approx - non-interactive fuzzy stream filter
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
#include <stdint.h>
#include <math.h>

#include "arg.h"
#include "approx.h"

#if defined(_WIN32) || defined(_WIN64)
static ssize_t
portable_getline(char **lineptr, size_t *n, FILE *stream)
{
	char *buf;
	size_t pos = 0;
	int c;

	if (!lineptr || !n || !stream) {
		errno = EINVAL;
		return -1;
	}

	if (!*lineptr || *n == 0) {
		*n = 128;
		*lineptr = malloc(*n);
		if (!*lineptr) {
			errno = ENOMEM;
			return -1;
		}
	}

	buf = *lineptr;

	while ((c = fgetc(stream)) != EOF) {
		if (pos + 2 >= *n) {
			size_t new_size = *n * 2;
			char *new_buf = realloc(*lineptr, new_size);
			if (!new_buf) {
				errno = ENOMEM;
				return -1;
			}
			*lineptr = new_buf;
			*n = new_size;
			buf = *lineptr;
		}
		buf[pos++] = (char)c;
		if (c == '\n')
			break;
	}

	if (pos == 0 && c == EOF)
		return -1;

	buf[pos] = '\0';
	return (ssize_t)pos;
}
#define getline portable_getline
#endif

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

static inline size_t
min3(size_t a, size_t b, size_t c)
{
	size_t m = a < b ? a : b;
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
	size_t buf_a[256], buf_b[256];
	size_t *prev, *curr, *tmp;
	size_t *alloc_a = NULL, *alloc_b = NULL;
	size_t min_dist, cost;
	size_t i, j;
	char c;

	if (patlen == 0)
		return 1.0;
	if (linelen == 0)
		return 0.0;

	if (patlen > SIZE_MAX / sizeof(size_t) - 1)
		die("approx: pattern too long");

	if (patlen + 1 <= sizeof(buf_a) / sizeof(buf_a[0])) {
		prev = buf_a;
		curr = buf_b;
	} else {
		alloc_a = malloc((patlen + 1) * sizeof(size_t));
		alloc_b = malloc((patlen + 1) * sizeof(size_t));
		if (!alloc_a || !alloc_b) {
			free(alloc_a);
			free(alloc_b);
			die("approx: out of memory");
		}
		prev = alloc_a;
		curr = alloc_b;
	}

	for (i = 0; i <= patlen; i++)
		prev[i] = i;

	min_dist = patlen;

	for (j = 0; j < linelen; j++) {
		c = line[j];
		curr[0] = 0;

		for (i = 1; i <= patlen; i++) {
			cost = char_eq(pat[i - 1], c, icase) ? 0 : 1;
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

	if (min_dist >= patlen)
		return 0.0;

	return 1.0 - ((double)min_dist / (double)patlen);
}

double
sim_exact(const char *pat, size_t patlen, const char *line, size_t linelen, int icase)
{
	size_t buf_a[256], buf_b[256];
	size_t *prev, *curr, *tmp;
	size_t *alloc_a = NULL, *alloc_b = NULL;
	size_t dist, max_len, cost;
	size_t i, j;
	char c;

	if (patlen == 0 && linelen == 0)
		return 1.0;
	if (patlen == 0 || linelen == 0)
		return 0.0;

	if (patlen > SIZE_MAX / sizeof(size_t) - 1)
		die("approx: pattern too long");

	if (patlen + 1 <= sizeof(buf_a) / sizeof(buf_a[0])) {
		prev = buf_a;
		curr = buf_b;
	} else {
		alloc_a = malloc((patlen + 1) * sizeof(size_t));
		alloc_b = malloc((patlen + 1) * sizeof(size_t));
		if (!alloc_a || !alloc_b) {
			free(alloc_a);
			free(alloc_b);
			die("approx: out of memory");
		}
		prev = alloc_a;
		curr = alloc_b;
	}

	for (i = 0; i <= patlen; i++)
		prev[i] = i;

	for (j = 0; j < linelen; j++) {
		c = line[j];
		curr[0] = j + 1;

		for (i = 1; i <= patlen; i++) {
			cost = char_eq(pat[i - 1], c, icase) ? 0 : 1;
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
	if (dist >= max_len)
		return 0.0;

	return 1.0 - ((double)dist / (double)max_len);
}

struct heap *
heap_create(size_t cap)
{
	struct heap *h;

	if (cap == 0 || cap > SIZE_MAX / sizeof(h->items[0]))
		die("approx: count too large");

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
	struct match_item tmp;
	size_t parent;

	while (idx > 0) {
		parent = (idx - 1) / 2;
		if (h->items[idx].score < h->items[parent].score) {
			tmp = h->items[idx];
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
	struct match_item tmp;
	size_t smallest, left, right;

	smallest = idx;
	left = 2 * idx + 1;
	right = 2 * idx + 2;

	if (left < h->size && h->items[left].score < h->items[smallest].score)
		smallest = left;
	if (right < h->size && h->items[right].score < h->items[smallest].score)
		smallest = right;

	if (smallest != idx) {
		tmp = h->items[idx];
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
	size_t lineno = 0, len;
	double score;
	int matched = 0, is_match;

	while ((linelen = getline(&line, &linecap, fp)) >= 0) {
		len = (size_t)linelen;
		lineno++;

		/* strip trailing newline and cr */
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
	FILE *fp;
	char *pat, *s, *end;
	size_t patlen, i;
	struct heap *h = NULL;
	int matched = 0, err = 0;

	ARGBEGIN {
	case 't':
		s = EARGF(usage());
		threshold = strtod(s, &end);
		if (*end != '\0' || isnan(threshold) || threshold < 0.0 || threshold > 1.0)
			die("approx: invalid threshold: %s (must be 0.0 to 1.0)", s);
		break;
	case 'n':
		s = EARGF(usage());
		opt_topn = strtol(s, &end, 10);
		if (*end != '\0' || opt_topn <= 0)
			die("approx: invalid count: %s", s);
		break;
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
		for (i = 0; i < (size_t)argc; i++) {
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
		heap_sort_descending(h);
		for (i = 0; i < h->size; i++) {
			if (opt_score)
				printf("%.2f\t%s\n", h->items[i].score, h->items[i].line);
			else
				puts(h->items[i].line);
		}
		heap_free(h);
	}

	if (fflush(stdout) == EOF || ferror(stdout)) {
		if (errno != EPIPE)
			die("approx: stdout:");
	}

	if (err)
		return err;

	return matched ? 0 : 1;
}
