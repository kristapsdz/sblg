/*	$Id$ */
/*
 * Copyright (c) 2014, 2017, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "extern.h"

static int
cmdlinecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	return(s1->order - s2->order);
}

static int
filenamecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	if (s1->sort != s2->sort) {
		if (SORT_FIRST == s1->sort || 
		    SORT_LAST == s2->sort)
			return(-1);
		else if (SORT_LAST == s1->sort || 
			 SORT_FIRST == s2->sort)
			return(1);
	}

	return(strcmp(s1->src, s2->src));
}

static int
rdatecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	if (s1->sort != s2->sort) {
		if (SORT_FIRST == s2->sort || 
		    SORT_LAST == s1->sort)
			return(-1);
		else if (SORT_LAST == s2->sort || 
			 SORT_FIRST == s1->sort)
			return(1);
	}

	return(difftime(s1->time, s2->time));
}

static int
datecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	if (s1->sort != s2->sort) {
		if (SORT_FIRST == s1->sort || 
		    SORT_LAST == s2->sort)
			return(-1);
		else if (SORT_LAST == s1->sort || 
			 SORT_FIRST == s2->sort)
			return(1);
	}

	return(difftime(s2->time, s1->time));
}

static void
article_free(struct article *p)
{
	size_t	 i;

	if (NULL == p) 
		return;

	free(p->img);
	free(p->base);
	free(p->stripbase);
	free(p->striplangbase);
	free(p->title);
	free(p->titletext);
	free(p->author);
	free(p->authortext);
	free(p->aside);
	free(p->asidetext);
	free(p->article);

	for (i = 0; i < p->tagmapsz; i++)
		free(p->tagmap[i]);
	for (i = 0; i < p->setmapsz; i++)
		free(p->setmap[i]);

	free(p->tagmap);
	free(p->setmap);
}

void
sblg_free(struct article *p, size_t sz)
{
	size_t	 i;

	if (NULL == p)
		return;

	for (i = 0; i < sz; i++)
		article_free(&p[i]);

	free(p);
}

void
sblg_sort(struct article *p, size_t sz, enum asort sort)
{

	if (ASORT_DATE == sort)
		qsort(p, sz, sizeof(struct article), datecmp);
	else if (ASORT_RDATE == sort)
		qsort(p, sz, sizeof(struct article), rdatecmp);
	else if (ASORT_FILENAME == sort)
		qsort(p, sz, sizeof(struct article), filenamecmp);
	else if (ASORT_CMDLINE == sort)
		qsort(p, sz, sizeof(struct article), cmdlinecmp);
}
