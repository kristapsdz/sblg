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

/*
 * All sort types can have the order overriden on an article-specific
 * basis by the SORT_FIRST or SORT_LAST override being applied.
 * Return non-zero if we do this; zero to fall through to our sort.
 */
static int
cmpoverride(const struct article *s1, const struct article *s2)
{

	if (s1->sort != s2->sort) {
		if (s2->sort == SORT_FIRST || s1->sort == SORT_LAST)
			return -1;
		if (s2->sort == SORT_LAST || s1->sort == SORT_FIRST)
			return 1;
	}
	return 0;
}

static int
rcmdlinecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	int	     rc;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return s2->order - s1->order;
}

static int
cmdlinecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	int	     rc;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return s1->order - s2->order;
}

static int
rfilenamecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	int	     rc;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return strcmp(s2->src, s1->src);
}

static int
filenamecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	int	     rc;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return strcmp(s1->src, s2->src);
}

static int
rititlecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	const char	*t1, *t2;
	int		 rc;

	t1 = (s1->titletext == NULL) ? "" : s1->titletext;
	t2 = (s2->titletext == NULL) ? "" : s2->titletext;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return strcasecmp(t2, t1);
}

static int
ititlecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	const char	*t1, *t2;
	int		 rc;

	t1 = (s1->titletext == NULL) ? "" : s1->titletext;
	t2 = (s2->titletext == NULL) ? "" : s2->titletext;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return strcasecmp(t1, t2);
}

static int
rtitlecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	const char	*t1, *t2;
	int		 rc;

	t1 = (s1->titletext == NULL) ? "" : s1->titletext;
	t2 = (s2->titletext == NULL) ? "" : s2->titletext;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return strcmp(t2, t1);
}

static int
titlecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	const char	*t1, *t2;
	int		 rc;

	t1 = (s1->titletext == NULL) ? "" : s1->titletext;
	t2 = (s2->titletext == NULL) ? "" : s2->titletext;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return strcmp(t1, t2);
}

static int
rdatecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	int	     rc;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return difftime(s1->time, s2->time);
}

static int
datecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;
	int	     rc;

	if ((rc = cmpoverride(s1, s2)) != 0)
		return rc;
	return difftime(s2->time, s1->time);
}

static void
article_free(struct article *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	free(p->img);
	free(p->base);
	free(p->src);
	free(p->stripsrc);
	free(p->stripbase);
	free(p->striplangbase);
	free(p->title);
	free(p->titletext);
	free(p->author);
	free(p->authortext);
	free(p->aside);
	free(p->asidetext);
	free(p->article);
	free(p->real);
	free(p->stripreal);
	free(p->realbase);
	free(p->striprealbase);
	free(p->striplangrealbase);

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

	if (p == NULL)
		return;

	for (i = 0; i < sz; i++)
		article_free(&p[i]);

	free(p);
}

/*
 * Look up the sort type.
 * Return zero if not found, non-zero if found.
 * Sets "sort" if found.
 */
int
sblg_sort_lookup(const char *s, enum asort *sort)
{

	if (strcasecmp(s, "date") == 0)
		*sort = ASORT_DATE;
	else if (strcasecmp(s, "rdate") == 0)
		*sort = ASORT_RDATE;
	else if (strcasecmp(s, "filename") == 0)
		*sort = ASORT_FILENAME;
	else if (strcasecmp(s, "rfilename") == 0)
		*sort = ASORT_RFILENAME;
	else if (strcasecmp(s, "cmdline") == 0)
		*sort = ASORT_CMDLINE;
	else if (strcasecmp(s, "rcmdline") == 0)
		*sort = ASORT_RCMDLINE;
	else if (strcasecmp(s, "title") == 0)
		*sort = ASORT_TITLE;
	else if (strcasecmp(s, "rtitle") == 0)
		*sort = ASORT_RTITLE;
	else if (strcasecmp(s, "ititle") == 0)
		*sort = ASORT_ITITLE;
	else if (strcasecmp(s, "rititle") == 0)
		*sort = ASORT_RITITLE;
	else
		return 0;

	return 1;
}

/*
 * Sort the list of articles in the manner given by "sort".
 * This will take into account per-article sort ordering.
 */
void
sblg_sort(struct article *p, size_t sz, enum asort sort)
{

	switch (sort) {
	case ASORT_DATE:
		qsort(p, sz, sizeof(struct article), datecmp);
		break;
	case ASORT_RDATE:
		qsort(p, sz, sizeof(struct article), rdatecmp);
		break;
	case ASORT_FILENAME:
		qsort(p, sz, sizeof(struct article), filenamecmp);
		break;
	case ASORT_RFILENAME:
		qsort(p, sz, sizeof(struct article), rfilenamecmp);
		break;
	case ASORT_CMDLINE:
		qsort(p, sz, sizeof(struct article), cmdlinecmp);
		break;
	case ASORT_RCMDLINE:
		qsort(p, sz, sizeof(struct article), rcmdlinecmp);
		break;
	case ASORT_TITLE:
		qsort(p, sz, sizeof(struct article), titlecmp);
		break;
	case ASORT_RTITLE:
		qsort(p, sz, sizeof(struct article), rtitlecmp);
		break;
	case ASORT_ITITLE:
		qsort(p, sz, sizeof(struct article), ititlecmp);
		break;
	case ASORT_RITITLE:
		qsort(p, sz, sizeof(struct article), rititlecmp);
		break;
	}
}
