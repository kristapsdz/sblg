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

#include <assert.h>
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
	size_t		 stack;
};

static	void	addr_data(struct parse *arg, const XML_Char **atts);
static	void	addr_end(void *userdata, const XML_Char *name);
static	void	addr_text(void *userdata, const XML_Char *s, int len);
static	void	article_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	head_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	head_end(void *userdata, const XML_Char *name);
static	void	input_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	time_data(struct parse *arg, const XML_Char **atts);
static	void	title_data(struct parse *arg, const XML_Char **atts);
static	void	title_end(void *userdata, const XML_Char *name);
static	void	title_text(void *userdata, const XML_Char *s, int len);

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
	XML_SetStartElementHandler(p, input_begin);
	XML_SetUserData(p, &parse);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)sz, 1)) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", src, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	if (NULL == parse.article->title) {
		parse.article->title = xstrdup("Untitled article");
		parse.article->titlesz = strlen(parse.article->title);
	}
	if (NULL == parse.article->author) {
		parse.article->author = xstrdup("Untitled author");
		parse.article->authorsz = strlen(parse.article->author);
	}
	if (0 == parse.article->time) {
		if (-1 == fstat(fd, &st)) {
			perror(src);
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
input_begin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "article"))
		XML_SetElementHandler(arg->p, article_begin, NULL);
}

static void
article_begin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "header"))
		XML_SetElementHandler(arg->p, head_begin, head_end);
}

static void
head_begin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "time"))
		time_data(arg, atts);
	else if (0 == strcasecmp(name, "address"))
		addr_data(arg, atts);
	else if (0 == strcasecmp(name, "h1"))
		title_data(arg, atts);
	else if (0 == strcasecmp(name, "h2"))
		title_data(arg, atts);
	else if (0 == strcasecmp(name, "h3"))
		title_data(arg, atts);
	else if (0 == strcasecmp(name, "h4"))
		title_data(arg, atts);
}

static void
addr_end(void *userdata, const XML_Char *name)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "address") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, head_begin, head_end);
		XML_SetCharacterDataHandler(arg->p, NULL);
	}
}

static void
head_end(void *userdata, const XML_Char *name)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "header"))
		XML_SetElementHandler(arg->p, NULL, NULL);
}

static void
title_data(struct parse *arg, const XML_Char **atts)
{

	if (NULL == arg->article->title) {
		XML_SetElementHandler(arg->p, NULL, title_end);
		XML_SetCharacterDataHandler(arg->p, title_text);
	}
}

static void
addr_text(void *userdata, const XML_Char *s, int len)
{
	struct parse	*arg = userdata;
	size_t		 sz;

	if (len <= 0)
		return;

	sz = arg->article->authorsz;
	arg->article->authorsz += (size_t)len;
	arg->article->author = xrealloc
		(arg->article->author, 
		 arg->article->authorsz + 1);
	memcpy(arg->article->author + sz, s, len);
	arg->article->author[arg->article->authorsz] = '\0';
}

static void
title_text(void *userdata, const XML_Char *s, int len)
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
title_end(void *userdata, const XML_Char *name)
{
	struct parse	*arg = userdata;

	if (0 == strcasecmp(name, "h1") ||
			0 == strcasecmp(name, "h2") ||
			0 == strcasecmp(name, "h3") ||
			0 == strcasecmp(name, "h4")) {
		XML_SetElementHandler(arg->p, head_begin, head_end);
		XML_SetCharacterDataHandler(arg->p, NULL);
	}
}

static void
addr_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct parse	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "address");
}

static void
addr_data(struct parse *arg, const XML_Char **atts)
{

	if (NULL == arg->article->author) {
		assert(0 == arg->stack);
		arg->stack++;
		XML_SetElementHandler(arg->p, addr_begin, addr_end);
		XML_SetCharacterDataHandler(arg->p, addr_text);
	}
}

static void
time_data(struct parse *arg, const XML_Char **atts)
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

void
grok_free(struct article *p)
{

	if (NULL != p) {
		free(p->title);
		free(p->author);
	}
}
