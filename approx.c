/*
 * approx - non-interactive fuzzy stream filter and ranker
 * See LICENSE file for copyright and license details.
 */

#define _POSIX_C_SOURCE 200809L
#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif

#define APPROX_IMPLEMENTATION
#include "approx.h"

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
		*lineptr = (char *)malloc(*n);
		if (!*lineptr) {
			errno = ENOMEM;
			return -1;
		}
	}

	buf = *lineptr;
	while ((c = fgetc(stream)) != EOF) {
		if (pos + 2 >= *n) {
			size_t new_size = *n * 2;
			char *new_buf = (char *)realloc(buf, new_size);
			if (!new_buf) {
				errno = ENOMEM;
				return -1;
			}
			buf = new_buf;
			*lineptr = buf;
			*n = new_size;
		}
		buf[pos++] = (char)c;
		if (c == '\n')
			break;
	}

	if (c == EOF && pos == 0)
		return -1;

	buf[pos] = '\0';
	return (ssize_t)pos;
}
#define getline portable_getline
#endif

char *argv0;

static double threshold = APPROX_DEFAULT_THRESHOLD;
static int opt_score = 0;
static int opt_flags = 0;
static int opt_invert = 0;
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
		new_pats = (char **)realloc(pl->patterns, new_cap * sizeof(char *));
		new_lens = (size_t *)realloc(pl->patlens, new_cap * sizeof(size_t));
		if (!new_pats || !new_lens)
			die("approx: out of memory");
		pl->patterns = new_pats;
		pl->patlens = new_lens;
		pl->cap = new_cap;
	}

	dup = (char *)malloc(len + 1);
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
process_stream(FILE *fp, const char *fname, int show_fname, const struct pattern_list *pl, approx_heap_t *h, size_t *count_out)
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
			approx_extract_field(line, len, opt_delim, opt_field, &match_tgt, &match_len);

		score = -1.0;
		best_mstart = 0;
		best_mend = 0;
		for (p = 0; p < pl->count; p++) {
			cur_score = approx_sim_span(pl->patterns[p], pl->patlens[p], match_tgt, match_len,
			                            opt_flags, &mstart, &mend);
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
					approx_heap_push(h, score, line, show_fname ? fname : NULL, best_mstart, best_mend, !opt_invert, lineno);
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
	approx_heap_t *h = NULL;
	int matched = 0, err = 0, show_fname;

	ARGBEGIN {
	case 'C':
		opt_color = 1;
		break;
	case 'F':
		opt_patfile = EARGF(usage());
		break;
	case 'D':
		opt_flags |= APPROX_DAMERAU;
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
		opt_flags |= APPROX_ICASE;
		break;
	case 'v':
		opt_invert = 1;
		break;
	case 'e':
		opt_flags |= APPROX_EXACT;
		break;
	case 'V':
		puts("approx-" APPROX_VERSION);
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

	if (opt_topn > 0 && !opt_quiet && !opt_count && !opt_files_with_matches && !opt_files_without_matches) {
		h = approx_heap_create((size_t)opt_topn);
		if (!h)
			die("approx: out of memory");
	}

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
		approx_heap_sort(h);
		for (i = 0; i < h->size; i++)
			print_match(h->items[i].meta, h->items[i].meta != NULL, h->items[i].score, h->items[i].line, h->items[i].mstart, h->items[i].mend, h->items[i].has_span);
		approx_heap_free(h);
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
