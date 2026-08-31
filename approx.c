/*
 * approx - non-interactive fuzzy stream filter
 * See LICENSE file for copyright and license details.
 */

#define _POSIX_C_SOURCE 200809L
#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif

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
static int opt_damerau = 0;
static int opt_quiet = 0;
static int opt_count = 0;
static int opt_color = 0;
static int opt_files_with_matches = 0;
static int opt_files_without_matches = 0;
static int opt_header = -1;
static long opt_topn = 0;
static long opt_max = 0;
static char opt_delim = '\0';
static long opt_field = 0;
static char *opt_patfile = NULL;

struct pattern_list {
	char **patterns;
	size_t *patlens;
	size_t count;
	size_t cap;
};

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
	fprintf(stderr, "usage: %s [-cCDhHilLmqsvV] [-t threshold] [-n count] [-m max] [-d delim] [-k field] [-F file] [pattern] [file ...]\n", argv0);
	exit(2);
}

static void
pattern_list_add(struct pattern_list *pl, const char *pat, size_t len)
{
	char *dup;
	char **new_pats;
	size_t *new_lens;
	size_t new_cap;

	if (pl->count >= pl->cap) {
		new_cap = pl->cap ? pl->cap * 2 : 8;
		new_pats = realloc(pl->patterns, new_cap * sizeof(char *));
		new_lens = realloc(pl->patlens, new_cap * sizeof(size_t));
		if (!new_pats || !new_lens)
			die("approx: out of memory");
		pl->patterns = new_pats;
		pl->patlens = new_lens;
		pl->cap = new_cap;
	}

	dup = malloc(len + 1);
	if (!dup)
		die("approx: out of memory");
	memcpy(dup, pat, len);
	dup[len] = '\0';

	pl->patterns[pl->count] = dup;
	pl->patlens[pl->count] = len;
	pl->count++;
}

static void
pattern_list_free(struct pattern_list *pl)
{
	size_t i;

	if (!pl)
		return;

	for (i = 0; i < pl->count; i++)
		free(pl->patterns[i]);

	free(pl->patterns);
	free(pl->patlens);
	pl->patterns = NULL;
	pl->patlens = NULL;
	pl->count = 0;
	pl->cap = 0;
}

static void
load_pattern_file(struct pattern_list *pl, const char *path)
{
	FILE *fp;
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	size_t len;

	fp = fopen(path, "r");
	if (!fp)
		die("approx: %s: %s", path, strerror(errno));

	while ((linelen = getline(&line, &linecap, fp)) >= 0) {
		len = (size_t)linelen;
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';
		pattern_list_add(pl, line, len);
	}

	free(line);
	fclose(fp);

	if (pl->count == 0)
		die("approx: %s: pattern file is empty", path);
}

static void
extract_field(const char *line, size_t linelen, char delim, long k, const char **fstart, size_t *flen)
{
	size_t i = 0, field_idx = 1, start = 0;

	if (k <= 0) {
		*fstart = line;
		*flen = linelen;
		return;
	}

	if (delim != '\0') {
		while (i <= linelen) {
			if (i == linelen || line[i] == delim) {
				if (field_idx == (size_t)k) {
					*fstart = line + start;
					*flen = i - start;
					return;
				}
				field_idx++;
				start = i + 1;
			}
			i++;
		}
	} else {
		while (i < linelen) {
			while (i < linelen && (line[i] == ' ' || line[i] == '\t'))
				i++;
			if (i == linelen)
				break;
			start = i;
			while (i < linelen && line[i] != ' ' && line[i] != '\t')
				i++;
			if (field_idx == (size_t)k) {
				*fstart = line + start;
				*flen = i - start;
				return;
			}
			field_idx++;
		}
	}

	*fstart = "";
	*flen = 0;
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
sim_substr(const char *pat, size_t patlen, const char *line, size_t linelen, int icase, int damerau, size_t *mstart, size_t *mend)
{
	size_t buf_a[256], buf_b[256], buf_c[256];
	size_t s_buf_a[256], s_buf_b[256], s_buf_c[256];
	size_t *pprev, *prev, *curr, *tmp;
	size_t *s_pprev, *s_prev, *s_curr, *s_tmp;
	size_t *alloc_a = NULL, *alloc_b = NULL, *alloc_c = NULL;
	size_t *s_alloc_a = NULL, *s_alloc_b = NULL, *s_alloc_c = NULL;
	size_t min_dist, cost;
	size_t best_start = 0, best_end = 0;
	size_t i, j, from_start;
	char c;

	if (patlen == 0) {
		if (mstart)
			*mstart = 0;
		if (mend)
			*mend = 0;
		return 1.0;
	}
	if (linelen == 0) {
		if (mstart)
			*mstart = 0;
		if (mend)
			*mend = 0;
		return 0.0;
	}

	if (patlen > SIZE_MAX / sizeof(size_t) - 1)
		die("approx: pattern too long");

	if (patlen + 1 <= sizeof(buf_a) / sizeof(buf_a[0])) {
		pprev = buf_a;
		prev = buf_b;
		curr = buf_c;
		s_pprev = s_buf_a;
		s_prev = s_buf_b;
		s_curr = s_buf_c;
	} else {
		alloc_a = malloc((patlen + 1) * sizeof(size_t));
		alloc_b = malloc((patlen + 1) * sizeof(size_t));
		alloc_c = malloc((patlen + 1) * sizeof(size_t));
		s_alloc_a = malloc((patlen + 1) * sizeof(size_t));
		s_alloc_b = malloc((patlen + 1) * sizeof(size_t));
		s_alloc_c = malloc((patlen + 1) * sizeof(size_t));
		if (!alloc_a || !alloc_b || !alloc_c || !s_alloc_a || !s_alloc_b || !s_alloc_c) {
			free(alloc_a); free(alloc_b); free(alloc_c);
			free(s_alloc_a); free(s_alloc_b); free(s_alloc_c);
			die("approx: out of memory");
		}
		pprev = alloc_a;
		prev = alloc_b;
		curr = alloc_c;
		s_pprev = s_alloc_a;
		s_prev = s_alloc_b;
		s_curr = s_alloc_c;
	}

	for (i = 0; i <= patlen; i++) {
		prev[i] = i;
		pprev[i] = i;
		s_prev[i] = 0;
		s_pprev[i] = 0;
	}

	min_dist = patlen;

	for (j = 0; j < linelen; j++) {
		c = line[j];
		curr[0] = 0;
		s_curr[0] = j;

		for (i = 1; i <= patlen; i++) {
			cost = char_eq(pat[i - 1], c, icase) ? 0 : 1;

			curr[i] = prev[i - 1] + cost;
			from_start = (i == 1) ? j : s_prev[i - 1];

			if (curr[i - 1] + 1 < curr[i]) {
				curr[i] = curr[i - 1] + 1;
				from_start = (i == 1) ? j : s_curr[i - 1];
			}

			if (prev[i] + 1 < curr[i]) {
				curr[i] = prev[i] + 1;
				from_start = s_prev[i];
			}

			if (damerau && j > 0 && i > 1 &&
			    char_eq(pat[i - 1], line[j - 1], icase) &&
			    char_eq(pat[i - 2], c, icase)) {
				if (pprev[i - 2] + 1 < curr[i]) {
					curr[i] = pprev[i - 2] + 1;
					from_start = (i == 2) ? (j - 1) : s_pprev[i - 2];
				}
			}

			s_curr[i] = from_start;
		}

		if (curr[patlen] < min_dist || (curr[patlen] == min_dist && min_dist < patlen)) {
			min_dist = curr[patlen];
			best_start = s_curr[patlen];
			best_end = j;
		}

		tmp = pprev;
		pprev = prev;
		prev = curr;
		curr = tmp;

		s_tmp = s_pprev;
		s_pprev = s_prev;
		s_prev = s_curr;
		s_curr = s_tmp;
	}

	if (alloc_a) {
		free(alloc_a); free(alloc_b); free(alloc_c);
		free(s_alloc_a); free(s_alloc_b); free(s_alloc_c);
	}

	if (mstart)
		*mstart = best_start;
	if (mend)
		*mend = best_end;

	if (min_dist >= patlen)
		return 0.0;

	return 1.0 - ((double)min_dist / (double)patlen);
}

double
sim_exact(const char *pat, size_t patlen, const char *line, size_t linelen, int icase, int damerau)
{
	size_t buf_a[256], buf_b[256], buf_c[256];
	size_t *pprev, *prev, *curr, *tmp;
	size_t *alloc_a = NULL, *alloc_b = NULL, *alloc_c = NULL;
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
		pprev = buf_a;
		prev = buf_b;
		curr = buf_c;
	} else {
		alloc_a = malloc((patlen + 1) * sizeof(size_t));
		alloc_b = malloc((patlen + 1) * sizeof(size_t));
		alloc_c = malloc((patlen + 1) * sizeof(size_t));
		if (!alloc_a || !alloc_b || !alloc_c) {
			free(alloc_a);
			free(alloc_b);
			free(alloc_c);
			die("approx: out of memory");
		}
		pprev = alloc_a;
		prev = alloc_b;
		curr = alloc_c;
	}

	for (i = 0; i <= patlen; i++) {
		prev[i] = i;
		pprev[i] = i;
	}

	for (j = 0; j < linelen; j++) {
		c = line[j];
		curr[0] = j + 1;

		for (i = 1; i <= patlen; i++) {
			cost = char_eq(pat[i - 1], c, icase) ? 0 : 1;
			curr[i] = min3(curr[i - 1] + 1,
			               prev[i] + 1,
			               prev[i - 1] + cost);
			if (damerau && j > 0 && i > 1 &&
			    char_eq(pat[i - 1], line[j - 1], icase) &&
			    char_eq(pat[i - 2], c, icase)) {
				if (pprev[i - 2] + 1 < curr[i])
					curr[i] = pprev[i - 2] + 1;
			}
		}

		tmp = pprev;
		pprev = prev;
		prev = curr;
		curr = tmp;
	}

	dist = prev[patlen];

	if (alloc_a) {
		free(alloc_a);
		free(alloc_b);
		free(alloc_c);
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

	for (i = 0; i < h->size; i++) {
		free(h->items[i].line);
		free(h->items[i].fname);
	}

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
heap_push(struct heap *h, double score, const char *line, const char *fname, size_t mstart, size_t mend, int has_span, size_t lineno)
{
	char *dup_line, *dup_fname = NULL;

	if (h->size < h->cap) {
		dup_line = strdup(line);
		if (!dup_line)
			die("approx: out of memory");
		if (fname) {
			dup_fname = strdup(fname);
			if (!dup_fname) {
				free(dup_line);
				die("approx: out of memory");
			}
		}
		h->items[h->size].score = score;
		h->items[h->size].line = dup_line;
		h->items[h->size].fname = dup_fname;
		h->items[h->size].mstart = mstart;
		h->items[h->size].mend = mend;
		h->items[h->size].has_span = has_span;
		h->items[h->size].lineno = lineno;
		h->size++;
		sift_up(h, h->size - 1);
	} else if (score > h->items[0].score) {
		dup_line = strdup(line);
		if (!dup_line)
			die("approx: out of memory");
		if (fname) {
			dup_fname = strdup(fname);
			if (!dup_fname) {
				free(dup_line);
				die("approx: out of memory");
			}
		}
		free(h->items[0].line);
		free(h->items[0].fname);
		h->items[0].score = score;
		h->items[0].line = dup_line;
		h->items[0].fname = dup_fname;
		h->items[0].mstart = mstart;
		h->items[0].mend = mend;
		h->items[0].has_span = has_span;
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

static void
print_match(const char *fname, int show_fname, double score, const char *line, size_t mstart, size_t mend, int has_span)
{
	size_t len = strlen(line);

	if (show_fname && fname)
		printf("%s:", fname);

	if (opt_score)
		printf("%.2f\t", score);

	if (opt_color && has_span && len > 0 && mend >= mstart && mend < len) {
		printf("%.*s\033[1;31m%.*s\033[0m%s\n",
		       (int)mstart, line,
		       (int)(mend + 1 - mstart), line + mstart,
		       line + mend + 1);
	} else {
		puts(line);
	}
}

static int
process_stream(FILE *fp, const char *fname, int show_fname, const struct pattern_list *pl, struct heap *h, size_t *count_out)
{
	char *line = NULL;
	const char *match_tgt;
	size_t linecap = 0;
	ssize_t linelen;
	size_t lineno = 0, len, match_len, matched_count = 0, p;
	size_t mstart, mend, best_mstart, best_mend;
	double score, cur_score;
	int matched = 0, is_match;

	while ((linelen = getline(&line, &linecap, fp)) >= 0) {
		len = (size_t)linelen;
		lineno++;

		/* strip trailing newline and cr */
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		match_tgt = line;
		match_len = len;

		if (opt_field > 0)
			extract_field(line, len, opt_delim, opt_field, &match_tgt, &match_len);

		score = -1.0;
		best_mstart = 0;
		best_mend = 0;
		for (p = 0; p < pl->count; p++) {
			if (opt_exact) {
				cur_score = sim_exact(pl->patterns[p], pl->patlens[p], match_tgt, match_len, opt_icase, opt_damerau);
				mstart = 0;
				mend = match_len > 0 ? match_len - 1 : 0;
			} else {
				cur_score = sim_substr(pl->patterns[p], pl->patlens[p], match_tgt, match_len, opt_icase, opt_damerau, &mstart, &mend);
			}
			if (cur_score > score) {
				score = cur_score;
				best_mstart = mstart;
				best_mend = mend;
			}
		}

		if (opt_field > 0 && match_tgt >= line) {
			size_t offset = (size_t)(match_tgt - line);
			best_mstart += offset;
			best_mend += offset;
		}

		is_match = opt_invert ? (score < threshold) : (score >= threshold);

		if (is_match) {
			matched = 1;
			matched_count++;
			if (opt_quiet)
				break;
			if (opt_files_with_matches) {
				puts(fname ? fname : "(standard input)");
				break;
			}
			if (!opt_count && !opt_files_without_matches) {
				if (h) {
					heap_push(h, score, line, show_fname ? fname : NULL, best_mstart, best_mend, !opt_invert, lineno);
				} else {
					print_match(fname, show_fname, score, line, best_mstart, best_mend, !opt_invert);
				}
			}
			if (opt_max > 0 && matched_count >= (size_t)opt_max)
				break;
		}
	}

	free(line);
	if (count_out)
		*count_out = matched_count;
	return matched;
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *s, *end;
	size_t i, cur_count, total_count = 0;
	struct pattern_list pl = {0};
	struct heap *h = NULL;
	int matched = 0, err = 0, show_fname;

	ARGBEGIN {
	case 'C':
		opt_color = 1;
		break;
	case 'F':
		opt_patfile = EARGF(usage());
		break;
	case 'D':
		opt_damerau = 1;
		break;
	case 'd':
		s = EARGF(usage());
		if (strlen(s) != 1)
			die("approx: delimiter must be a single character: %s", s);
		opt_delim = s[0];
		break;
	case 'k':
		s = EARGF(usage());
		opt_field = strtol(s, &end, 10);
		if (*end != '\0' || opt_field <= 0)
			die("approx: invalid field: %s", s);
		break;
	case 'c':
		opt_count = 1;
		break;
	case 'q':
		opt_quiet = 1;
		break;
	case 'l':
		opt_files_with_matches = 1;
		break;
	case 'L':
		opt_files_without_matches = 1;
		break;
	case 'H':
		opt_header = 1;
		break;
	case 'h':
		opt_header = 0;
		break;
	case 'm':
		s = EARGF(usage());
		opt_max = strtol(s, &end, 10);
		if (*end != '\0' || opt_max <= 0)
			die("approx: invalid count: %s", s);
		break;
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

	if (opt_patfile) {
		load_pattern_file(&pl, opt_patfile);
	} else {
		if (argc < 1)
			usage();
		pattern_list_add(&pl, argv[0], strlen(argv[0]));
		argc--;
		argv++;
	}

	show_fname = (opt_header == 1);

	if (opt_topn > 0 && !opt_quiet && !opt_count && !opt_files_with_matches && !opt_files_without_matches)
		h = heap_create((size_t)opt_topn);

	if (argc == 0) {
		const char *fname = (show_fname) ? "(standard input)" : NULL;
		if (process_stream(stdin, fname, show_fname, &pl, h, &cur_count))
			matched = 1;
		if (opt_files_without_matches && cur_count == 0 && !opt_quiet)
			puts("(standard input)");
		if (opt_count && !opt_quiet && !opt_files_with_matches && !opt_files_without_matches) {
			if (show_fname && fname)
				printf("%s:%lu\n", fname, (unsigned long)cur_count);
			else
				printf("%lu\n", (unsigned long)cur_count);
		}
	} else {
		for (i = 0; i < (size_t)argc; i++) {
			const char *fname = argv[i];
			if (strcmp(fname, "-") == 0) {
				fp = stdin;
				fname = (show_fname) ? "(standard input)" : NULL;
			} else {
				fp = fopen(argv[i], "r");
				if (!fp) {
					fprintf(stderr, "approx: %s: %s\n", argv[i], strerror(errno));
					err = 2;
					continue;
				}
			}

			if (process_stream(fp, argv[i], show_fname, &pl, h, &cur_count))
				matched = 1;

			if (opt_files_without_matches && cur_count == 0 && !opt_quiet)
				puts(argv[i]);

			if (opt_count && !opt_quiet && !opt_files_with_matches && !opt_files_without_matches) {
				if (show_fname)
					printf("%s:%lu\n", argv[i], (unsigned long)cur_count);
				else
					total_count += cur_count;
			}

			if (fp != stdin)
				fclose(fp);

			if (opt_quiet && matched)
				break;
		}
		if (opt_count && !show_fname && !opt_quiet && !opt_files_with_matches && !opt_files_without_matches)
			printf("%lu\n", (unsigned long)total_count);
	}

	if (h) {
		heap_sort_descending(h);
		for (i = 0; i < h->size; i++)
			print_match(h->items[i].fname, h->items[i].fname != NULL, h->items[i].score, h->items[i].line, h->items[i].mstart, h->items[i].mend, h->items[i].has_span);
		heap_free(h);
	}

	pattern_list_free(&pl);

	if (fflush(stdout) == EOF || ferror(stdout)) {
		if (errno != EPIPE)
			die("approx: stdout:");
	}

	if (err)
		return err;

	return matched ? 0 : 1;
}
