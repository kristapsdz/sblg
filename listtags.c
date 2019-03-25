/*	$Id$ */
/*
 * Copyright (c) 2016--2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
	const char	  *fn;
	TAILQ_ENTRY(filen) entries;
};

TAILQ_HEAD(filenq, filen);

struct	tagn {
	struct filenq	 *fq;
	char	 	 *tag;
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
 
	for ( ; '\0' != *cp; cp++)
		if ( ! ('\\' == cp[0] && ' ' == cp[1]))
			putchar(*cp);
}

/*
 * Print tags in tag-major ordering.
 * This will print the articles referencing individual tags.
 * This is more complicated because our data comes in article-major
 * ordering so we need to hash entries and bucket.
 */
static int
dorlist(const struct article *sargs, size_t sargsz, int json)
{
	size_t	 	 i, j;
	char		*copy;
	ENTRY		 e;
	ENTRY		*ep;
	struct filenq	*fq;
	struct filen	*fn;
	struct tagnq	 tq;
	struct tagn	*tn;

	/*
	 * Start by creating a table of all tags and the files that
	 * reference those tags.
	 * Use the shitty hcreate() interface.
	 */

	if ( ! hcreate(256)) {
		warnx("hcreate");
		return(0);
	}

	TAILQ_INIT(&tq);

	for (i = 0; i < sargsz; i++) {
		for (j = 0; j < sargs[i].tagmapsz; j++) {
			e.key = sargs[i].tagmap[j];
			e.data = NULL;
			if (NULL == (ep = hsearch(e, FIND))) {
				fq = xmalloc(sizeof(struct filenq));
				TAILQ_INIT(fq);
				e.data = fq;
				copy = xstrdup(sargs[i].tagmap[j]);
				e.key = copy;
				if (NULL == (ep = hsearch(e, ENTER))) {
					warnx("hsearch");
					return(0);
				}
				tn = xmalloc(sizeof(struct tagn));
				tn->tag = copy;
				tn->fq = fq;
				TAILQ_INSERT_TAIL(&tq, tn, entries);
			}
			fq = ep->data;
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
		TAILQ_FOREACH(fn, tn->fq, entries) {
			if (json)
				putchar('"');
			else
				printf("%s\t", tn->tag);
			printf("%s", fn->fn);
			if (json)
				putchar('"');
			else
				putchar('\n');
			if (json && TAILQ_NEXT(fn, entries))
				putchar(',');
		}
		if (json && TAILQ_NEXT(tn, entries))
			puts("]},");
		else if (json)
			puts("]}");
	}

	/*
	 * Clean up the hashtable entries.
	 * XXX: on Linux, the "key" value to the ENTER-created table
	 * entries is not destroyed, so we do that later.
	 */

	hdestroy();

	while (NULL != (tn = TAILQ_FIRST(&tq))) {
		while (NULL != (fn = TAILQ_FIRST(tn->fq))) {
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

	return(1);
}

/*
 * Print article-major ordering.
 * This prints the tags belonging to each article.
 */
static int
dolist(const struct article *sargs, size_t sargsz, int json)
{
	size_t	 	 i, j;

	for (i = 0; i < sargsz; i++) {
		if (json)
			printf("{\"src\": \"%s\", \n"
			       " \"tags\": [", sargs[i].src);
		for (j = 0; j < sargs[i].tagmapsz; j++) {
			if (json && j > 0)
				putchar(',');
			if (json)
				putchar('"');
			else
				printf("%s\t", sargs[i].src);
			unescape(sargs[i].tagmap[j]);
			if (json)
				putchar('"');
			else
				putchar('\n');
		}
		if (json && i < sargsz - 1)
			puts("]},");
		else if (json)
			puts("]}");
	}

	return(1);
}

int
listtags(XML_Parser p, int sz, char *src[], int json, int reverse)
{
	size_t		 sargsz = 0;
	int		 i, rc;
	struct article	*sargs = NULL;

	/* First run the initial parse of all files. */

	for (i = 0; i < sz; i++) 
		if ( ! sblg_parse(p, src[i], &sargs, &sargsz, NULL)) {
			sblg_free(sargs, sargsz);
			return(0);
		}

	/* Now actually emit the listings. */

	if (json)
		puts("{[");

	rc = reverse ?
		dorlist(sargs, sargsz, json) :
		dolist(sargs, sargsz, json);

	if (json)
		puts("]}");

	/* Cleanup and exit. */

	sblg_free(sargs, sargsz);
	return(rc);
}

