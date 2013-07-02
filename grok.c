/*	$Id$ */
/*
 * Copyright (c) 2013 Kristaps Dzonsons <kristaps@bsd.lv>,
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
#include <sys/stat.h>

#include <err.h>
#include <expat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "extern.h"

struct	parse {
	XML_Parser	 p;
	struct article	*article;
};

static	void	elem_abegin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	elem_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	elem_bbegin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	elem_bend(void *userdata, const XML_Char *name);
static	void	elem_time(struct parse *arg, const XML_Char **atts);
static	void	elem_h(struct parse *arg, const XML_Char **atts);
static	void	elem_hend(void *userdata, const XML_Char *name);
static	void	elem_happend(void *userdata, const XML_Char *s, int len);

int
grok(XML_Parser p, const char *src, struct article *arg)
{
	char		*buf;
	size_t		 sz;
	int		 fd, rc;
	struct parse	 parse;
	struct stat	 st;

	memset(arg, 0, sizeof(struct article));
	memset(&parse, 0, sizeof(struct parse));

	rc = 0;

	if ( ! mmap_open(src, &fd, &buf, &sz))
		goto out;

	arg->src = src;
	parse.article = arg;
	parse.p = p;

	XML_ParserReset(p, NULL);
	XML_SetStartElementHandler(p, elem_abegin);
	XML_SetUserData(p, &parse);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)sz, 1)) {
		warnx("%s: %s", src, 
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	if (NULL == parse.article->title) {
		parse.article->title = xstrdup("Untitled article");
		parse.article->titlesz = strlen(parse.article->title);
	}
	if (0 == parse.article->time) {
		if (-1 == fstat(fd, &st)) {
			warn("%s", src);
			goto out;
		}
		parse.article->time = st.st_ctime;
	}

	rc = 1;
out:
	mmap_close(fd, buf, sz);
	return(rc);
}

static void
elem_abegin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "article"))
		XML_SetElementHandler(arg->p, elem_begin, NULL);
}

static void
elem_begin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "header"))
		XML_SetElementHandler(arg->p, elem_bbegin, elem_bend);
}

static void
elem_bbegin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "time"))
		elem_time(arg, atts);
	else if (0 == strcasecmp(name, "h1"))
		elem_h(arg, atts);
	else if (0 == strcasecmp(name, "h2"))
		elem_h(arg, atts);
	else if (0 == strcasecmp(name, "h3"))
		elem_h(arg, atts);
	else if (0 == strcasecmp(name, "h4"))
		elem_h(arg, atts);
}

static void
elem_bend(void *userdata, const XML_Char *name)
{
	struct parse	*arg = userdata;

	if (strcasecmp(name, "header"))
		return;

	XML_SetElementHandler(arg->p, NULL, NULL);
}

static void
elem_h(struct parse *arg, const XML_Char **atts)
{

	if (NULL != arg->article->title)
		return;

	XML_SetElementHandler(arg->p, NULL, elem_hend);
	XML_SetCharacterDataHandler(arg->p, elem_happend);
}

static void
elem_happend(void *userdata, const XML_Char *s, int len)
{
	struct parse	*arg = userdata;
	size_t		 sz;

	if (len <= 0)
		return;

	sz = arg->article->titlesz;
	arg->article->titlesz += (size_t)len;

	arg->article->title = xrealloc
		(arg->article->title, 
		 arg->article->titlesz + 1);
	memcpy(arg->article->title + sz, s, len);
	arg->article->title[arg->article->titlesz] = '\0';
}

static void
elem_hend(void *userdata, const XML_Char *name)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "h1") ||
			0 == strcasecmp(name, "h2") ||
			0 == strcasecmp(name, "h3") ||
			0 == strcasecmp(name, "h4")) {
		XML_SetElementHandler(arg->p, elem_bbegin, elem_bend);
		XML_SetCharacterDataHandler(arg->p, NULL);
	}
}

static void
elem_time(struct parse *arg, const XML_Char **atts)
{
	struct tm	 tm;

	for ( ; NULL != *atts; atts += 2) {
		if (strcasecmp(atts[0], "datetime"))
			continue;
		memset(&tm, 0, sizeof(struct tm));
		if (NULL == strptime(atts[1], "%Y-%m-%d", &tm))
			continue;
		arg->article->time = mktime(&tm);
	}
}
