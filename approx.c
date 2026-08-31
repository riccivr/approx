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
static int opt_quiet = 0;
static int opt_count = 0;
static int opt_files_with_matches = 0;
static int opt_files_without_matches = 0;
static int opt_header = -1;
static long opt_topn = 0;
static long opt_max = 0;
static char opt_delim = '\0';
static long opt_field = 0;

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
	fprintf(stderr, "usage: %s [-cehHilLmqsvV] [-t threshold] [-n count] [-m max] [-d delim] [-k field] pattern [file ...]\n", argv0);
	exit(2);
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
heap_push(struct heap *h, double score, const char *line, const char *fname, size_t lineno)
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
print_match(const char *fname, int show_fname, double score, const char *line)
{
	if (show_fname && fname) {
		if (opt_score)
			printf("%s:%.2f\t%s\n", fname, score, line);
		else
			printf("%s:%s\n", fname, line);
	} else {
		if (opt_score)
			printf("%.2f\t%s\n", score, line);
		else
			puts(line);
	}
}

static int
process_stream(FILE *fp, const char *fname, int show_fname, const char *pat, size_t patlen, struct heap *h, size_t *count_out)
{
	char *line = NULL;
	const char *match_tgt;
	size_t linecap = 0;
	ssize_t linelen;
	size_t lineno = 0, len, match_len, matched_count = 0;
	double score;
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

		if (opt_exact)
			score = sim_exact(pat, patlen, match_tgt, match_len, opt_icase);
		else
			score = sim_substr(pat, patlen, match_tgt, match_len, opt_icase);

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
					heap_push(h, score, line, show_fname ? fname : NULL, lineno);
				} else {
					print_match(fname, show_fname, score, line);
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
	char *pat, *s, *end;
	size_t patlen, i, cur_count, total_count = 0;
	struct heap *h = NULL;
	int matched = 0, err = 0, show_fname;

	ARGBEGIN {
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

	if (argc < 1)
		usage();

	pat = argv[0];
	patlen = strlen(pat);
	argc--;
	argv++;

	show_fname = (opt_header == 1);

	if (opt_topn > 0 && !opt_quiet && !opt_count && !opt_files_with_matches && !opt_files_without_matches)
		h = heap_create((size_t)opt_topn);

	if (argc == 0) {
		const char *fname = (show_fname) ? "(standard input)" : NULL;
		if (process_stream(stdin, fname, show_fname, pat, patlen, h, &cur_count))
			matched = 1;
		if (opt_files_without_matches && cur_count == 0 && !opt_quiet)
			puts("(standard input)");
		if (opt_count && !opt_quiet && !opt_files_with_matches && !opt_files_without_matches) {
			if (show_fname && fname)
				printf("%s:%zu\n", fname, cur_count);
			else
				printf("%zu\n", cur_count);
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

			if (process_stream(fp, argv[i], show_fname, pat, patlen, h, &cur_count))
				matched = 1;

			if (opt_files_without_matches && cur_count == 0 && !opt_quiet)
				puts(argv[i]);

			if (opt_count && !opt_quiet && !opt_files_with_matches && !opt_files_without_matches) {
				if (show_fname)
					printf("%s:%zu\n", argv[i], cur_count);
				else
					total_count += cur_count;
			}

			if (fp != stdin)
				fclose(fp);

			if (opt_quiet && matched)
				break;
		}
		if (opt_count && !show_fname && !opt_quiet && !opt_files_with_matches && !opt_files_without_matches)
			printf("%zu\n", total_count);
	}

	if (h) {
		heap_sort_descending(h);
		for (i = 0; i < h->size; i++)
			print_match(h->items[i].fname, h->items[i].fname != NULL, h->items[i].score, h->items[i].line);
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
