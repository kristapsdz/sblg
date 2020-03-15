/*	$Id$ */
/*
 * Copyright (c) 2016--2017, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <expat.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

struct	filen {
	const char	  *fn; /* name of file */
	TAILQ_ENTRY(filen) entries;
};

TAILQ_HEAD(filenq, filen);

struct	tagn {
	struct filenq	 *fq; /* files referencing tag */
	char	 	 *tag; /* name of tag */
	TAILQ_ENTRY(tagn) entries;
};

TAILQ_HEAD(tagnq, tagn);

/*
 * Strip escaped white-space.
 * XXX: should we do any escaping here?
 */
static void
unescape(const char *cp)
{
 
	for ( ; *cp != '\0'; cp++)
		if (!(cp[0] == '\\' && cp[1] == ' '))
			putchar(*cp);
}

/*
 * Print tags in tag-major ordering.
 * This will print the articles referencing individual tags.
 * This is more complicated because our data comes in article-major
 * ordering so we need to hash entries and bucket.
 */
static void
dorlist(const struct article *sargs, size_t sargsz, int json, int lf)
{
	size_t	 	 i, j;
	char		*copy;
	ENTRY		 e;
	ENTRY		*ep;
	struct filenq	*fq;
	struct filen	*fn;
	struct tagnq	 tq;
	struct tagn	*tn;

	TAILQ_INIT(&tq);

	/*
	 * Start by creating a table of all tags and the files that
	 * reference those tags.
	 * Use the shitty hcreate() interface.
	 */

	if (!hcreate(256))
		errx(EXIT_FAILURE, "hcreate");

	for (i = 0; i < sargsz; i++) {
		for (j = 0; j < sargs[i].tagmapsz; j++) {
			e.key = sargs[i].tagmap[j];
			e.data = NULL;
			if ((ep = hsearch(e, FIND)) == NULL) {
				fq = xmalloc(sizeof(struct filenq));
				TAILQ_INIT(fq);
				e.data = (char *)fq;
				copy = xstrdup(sargs[i].tagmap[j]);
				e.key = copy;
				if ((ep = hsearch(e, ENTER)) == NULL)
					errx(EXIT_FAILURE, "hsearch");
				tn = xmalloc(sizeof(struct tagn));
				tn->tag = copy;
				tn->fq = fq;
				TAILQ_INSERT_TAIL(&tq, tn, entries);
			}
			fq = (struct filenq *)ep->data;
			fn = xmalloc(sizeof(struct filen));
			fn->fn = sargs[i].src;
			TAILQ_INSERT_TAIL(fq, fn, entries);
		}
	}

	TAILQ_FOREACH(tn, &tq, entries) {
		if (json) {
			printf("{\"tag\": \"");
			unescape(tn->tag);
			printf("\", \n \"srcs\": [");
		}
		if (lf)
			unescape(tn->tag);
		TAILQ_FOREACH(fn, tn->fq, entries) {
			if (json)
				putchar('"');
			if (!json && lf)
				putchar('\t');
			if (!json && !lf)
				printf("%s\t", tn->tag);
			printf("%s", fn->fn);
			if (json)
				putchar('"');
			if (!json && !lf)
				putchar('\n');
			if (json && TAILQ_NEXT(fn, entries) != NULL)
				putchar(',');
		}
		if (json && TAILQ_NEXT(tn, entries) != NULL)
			puts("]},");
		else if (json)
			puts("]}");
		if (!json && lf)
			puts("");
	}

	/*
	 * XXX: on Linux, the "key" value to the ENTER-created table
	 * entries is not destroyed, so we do that later.
	 */

	hdestroy();

	while ((tn = TAILQ_FIRST(&tq)) != NULL) {
		while ((fn = TAILQ_FIRST(tn->fq)) != NULL) {
			TAILQ_REMOVE(tn->fq, fn, entries);
			free(fn);
		}
		TAILQ_REMOVE(&tq, tn, entries);
#ifdef __linux__
		free(tn->tag);
#endif
		free(tn->fq);
		free(tn);
	}
}

/*
 * Print article-major ordering.
 * This prints the tags belonging to each article.
 */
static void
dolist(const struct article *sargs, size_t sargsz, int json, int lf)
{
	size_t	 i, j;

	for (i = 0; i < sargsz; i++) {
		if (json)
			printf("{\"src\": \"%s\", \n"
			       " \"tags\": [", sargs[i].src);
		else if (lf)
			printf("%s", sargs[i].src);

		for (j = 0; j < sargs[i].tagmapsz; j++) {
			if (json && j > 0)
				putchar(',');
			if (json)
				putchar('"');
			if (!json && lf)
				putchar('\t');
			if (!json && !lf)
				printf("%s\t", sargs[i].src);
			unescape(sargs[i].tagmap[j]);
			if (json)
				putchar('"');
			if (!json && !lf)
				putchar('\n');
		}
		if (json && i < sargsz - 1)
			puts("]},");
		else if (json)
			puts("]}");
		else if (lf)
			puts("");
	}
}

/*
 * Emit all of the tags in no particular order, file-first or tag-first
 * is "reverse" was specified.
 * If json is specified, format the output in JSON.
 * If long is specified, have the matched file or tags be all on one
 * line instead of one per line.
 * Returns zero on failure, non-zero on success.
 */
int
listtags(XML_Parser p, int sz, char *src[],
	int json, int reverse, int longformat)
{
	size_t		 sargsz = 0;
	int		 i;
	struct article	*sargs = NULL;

	for (i = 0; i < sz; i++) 
		if (!sblg_parse(p, src[i], &sargs, &sargsz, NULL)) {
			sblg_free(sargs, sargsz);
			return 0;
		}

	sblg_destroy();

	if (json)
		puts("{[");
	if (reverse)
		dorlist(sargs, sargsz, json, longformat);
	else
		dolist(sargs, sargsz, json, longformat);
	if (json)
		puts("]}");

	sblg_free(sargs, sargsz);
	return 1;
}
