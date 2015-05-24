/*	$Id$ */
/*
 * Copyright (c) 2013, 2014 Kristaps Dzonsons <kristaps@bsd.lv>,
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
	struct article	*article; /* article being parsed */
	size_t		 stack; /* stack (many uses) */
	size_t		 gstack; /* global "article" stack */
#define	PARSE_ASIDE	 1 /* we've seen an aside */
#define	PARSE_TIME	 2 /* we've seen a time */
#define	PARSE_ADDR	 4 /* we've seen an address */
#define	PARSE_TITLE	 8 /* we've seen a title */
	unsigned int	 flags;
};

/*
 * Forward declarations for circular references.
 */
static void	article_begin(void *dat, const XML_Char *s, 
			const XML_Char **atts);
static void	article_end(void *dat, const XML_Char *s);

static void
article_text(void *dat, const XML_Char *s, int len)
{
	struct parse	*arg = dat;

	xmlstrtext(&arg->article->article, 
		&arg->article->articlesz, s, len);
}

static void
title_text(void *dat, const XML_Char *s, int len)
{
	struct parse	*arg = dat;

	xmlstrtext(&arg->article->title, 
		&arg->article->titlesz, s, len);
	xmlstrtext(&arg->article->titletext, 
		&arg->article->titletextsz, s, len);
	article_text(dat, s, len);
}

static void
addr_text(void *dat, const XML_Char *s, int len)
{
	struct parse	*arg = dat;

	xmlstrtext(&arg->article->author, 
		&arg->article->authorsz, s, len);
	xmlstrtext(&arg->article->authortext, 
		&arg->article->authortextsz, s, len);
	article_text(dat, s, len);
}

static void
aside_text(void *dat, const XML_Char *s, int len)
{
	struct parse	*arg = dat;

	xmlstrtext(&arg->article->aside, 
		&arg->article->asidesz, s, len);
	xmlstrtext(&arg->article->asidetext, 
		&arg->article->asidetextsz, s, len);
	article_text(dat, s, len);
}

static void
title_end(void *dat, const XML_Char *s)
{
	struct parse	*arg = dat;

	xmlstrclose(&arg->article->article,
		&arg->article->articlesz, s);

	if (0 == strcasecmp(s, "h1") ||
			0 == strcasecmp(s, "h2") ||
			0 == strcasecmp(s, "h3") ||
			0 == strcasecmp(s, "h4")) {
		XML_SetElementHandler(arg->p, 
			article_begin, article_end);
		XML_SetDefaultHandlerExpand(arg->p, article_text);
	} else
		xmlstrclose(&arg->article->title, 
			&arg->article->titlesz, s);
}

static void
aside_end(void *dat, const XML_Char *s)
{
	struct parse	*arg = dat;

	xmlstrclose(&arg->article->article,
		&arg->article->articlesz, s);

	if (0 == strcasecmp(s, "aside") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, 
			article_begin, article_end);
		XML_SetDefaultHandlerExpand(arg->p, article_text);
	} else
		xmlstrclose(&arg->article->aside, 
			&arg->article->asidesz, s);
}

static void
addr_end(void *dat, const XML_Char *s)
{
	struct parse	*arg = dat;

	xmlstrclose(&arg->article->article,
		&arg->article->articlesz, s);

	if (0 == strcasecmp(s, "address") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, 
			article_begin, article_end);
		XML_SetDefaultHandlerExpand(arg->p, article_text);
	} else
		xmlstrclose(&arg->article->author, 
			&arg->article->authorsz, s);
}

static void
tsearch(struct parse *arg, const XML_Char *s, const XML_Char **atts)
{
	const XML_Char	**attp;
	size_t		  sz;

	for (attp = atts; NULL != *attp; attp += 2)
		if (0 == strcasecmp(*attp, "data-sblg-tags")) {
			if (0 == (sz = strlen(attp[1])))
				continue;
			sz += arg->article->tagsz + 2;
			arg->article->tags = xrealloc
				(arg->article->tags, sz);
			if (0 == arg->article->tagsz)
				*arg->article->tags = '\0';
			strlcat(arg->article->tags, attp[1], sz);
			strlcat(arg->article->tags, " ", sz);
			arg->article->tagsz = sz;
		}
}

static void
title_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	*arg = dat;

	arg->stack += 0 == strcasecmp(s, "title");
	xmlstropen(&arg->article->title, 
		&arg->article->titlesz, s, atts);
	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts);
	tsearch(arg, s, atts);
}


static void
addr_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	*arg = dat;

	arg->stack += 0 == strcasecmp(s, "address");
	xmlstropen(&arg->article->author, 
		&arg->article->authorsz, s, atts);
	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts);
	tsearch(arg, s, atts);
}

static void
aside_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	*arg = dat;

	arg->stack += 0 == strcasecmp(s, "aside");
	xmlstropen(&arg->article->aside, 
		&arg->article->asidesz, s, atts);
	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts);
	tsearch(arg, s, atts);
}

/*
 * Look for a few important parts of the article: the header, the aside,
 * nested articles, etc.
 * This is where we'll actually grok the data.
 */
static void
article_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	 *arg = dat;
	const XML_Char	**attp;
	struct tm	  tm;

	assert(0 == arg->stack);

	xmlstropen(&arg->article->article,
		&arg->article->articlesz, s, atts);
	tsearch(arg, s, atts);

	if (0 == strcasecmp(s, "aside")) {
		if (PARSE_ASIDE & arg->flags)
			return;
		arg->stack++;
		arg->flags |= PARSE_ASIDE;
		XML_SetDefaultHandlerExpand(arg->p, aside_text);
		XML_SetElementHandler(arg->p, aside_begin, aside_end);
	} else if (0 == strcasecmp(s, "time")) {
		if (PARSE_TIME & arg->flags)
			return;
		arg->flags |= PARSE_TIME;
		for (attp = atts; NULL != *attp; attp += 2) {
			if (strcasecmp(atts[0], "datetime"))
				continue;
			memset(&tm, 0, sizeof(struct tm));
			if (NULL == strptime(atts[1], "%F", &tm))
				continue;
			arg->article->time = mktime(&tm);
		}
	} else if (0 == strcasecmp(s, "address")) {
		if (PARSE_ADDR & arg->flags) 
			return;
		arg->flags |= PARSE_ADDR;
		assert(0 == arg->stack);
		arg->stack++;
		XML_SetElementHandler(arg->p, addr_begin, addr_end);
		XML_SetDefaultHandlerExpand(arg->p, addr_text);
	} else if (0 == strcasecmp(s, "h1") ||
			0 == strcasecmp(s, "h2") ||
			0 == strcasecmp(s, "h3") ||
			0 == strcasecmp(s, "h4")) {
		if (PARSE_TITLE & arg->flags) 
			return;
		arg->flags |= PARSE_TITLE;
		XML_SetElementHandler(arg->p, title_begin, title_end);
		XML_SetDefaultHandlerExpand(arg->p, title_text);
	} else if (0 == strcasecmp(s, "article"))
		arg->gstack++;
}

static void
article_end(void *dat, const XML_Char *s)
{
	struct parse	*arg = dat;

	xmlstrclose(&arg->article->article,
		&arg->article->articlesz, s);

	if (0 == strcasecmp(s, "article") && 0 == --arg->gstack) {
		XML_SetElementHandler(arg->p, NULL, NULL);
		XML_SetDefaultHandlerExpand(arg->p, NULL);
	} 
}

/*
 * Look for the first instance of <article>.
 * If we're linking files, it must consist of the "data-sblg-article",
 * otherwise this isn't necessary.
 */
static void
input_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	 *arg = dat;
	char		 *cp, *tok, *tofree, *loc, *start;
	char		  c;
	size_t		  sz;
	const XML_Char	**attp;

	assert(0 == arg->gstack);
	assert(0 == arg->stack);

	if (strcasecmp(s, "article"))
		return;

	/* Look for the data-sblg-article mention.  */
	for (attp = atts; NULL != *attp; attp += 2)
		if (0 == strcasecmp(*attp, "data-sblg-article"))
			break;

	/*
	 * If we don't have the data-sblg-article (or it's set to
	 * false), then continue on.
	 */
	if (NULL == *attp || ! xmlbool(attp[1]))
		return;

	/*
	 * If we have any languages specified, append them here.
	 */
	for (attp = atts; NULL != *attp; attp += 2) 
		if (0 == strcasecmp(*attp, "data-sblg-lang")) {
			cp = tofree = xstrdup(attp[1]);
			while ((tok = strsep(&cp, " \t")) != NULL) {
				if ('\0' == *tok)
					continue;
				start = strrchr(arg->article->base, '/');
				if (NULL == start)
					start = arg->article->base;
				else
					start++;
				loc = strstr(start, tok);
				if (NULL == loc || loc == start)
					continue;
				if ('.' != loc[-1])
					continue;
				c = loc[strlen(tok)];
				if ('.' != c && '\0' != c)
					continue;
				sz = strlen(loc) - strlen(tok) + 1;
				memmove(loc - 1, loc + strlen(tok), sz);
			}
			free(tofree);
		}

	arg->gstack = 1;
	xmlstropen(&arg->article->article, &arg->article->articlesz, s, atts);
	XML_SetElementHandler(arg->p, article_begin, article_end);
	XML_SetDefaultHandlerExpand(arg->p, article_text);
	tsearch(arg, s, atts);
}

int
grok(XML_Parser p, const char *src, struct article *arg)
{
	char		*buf, *cp;
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
	arg->base = xstrdup(src);
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

	if (NULL != (cp = strrchr(arg->base, '.')))
		if (NULL == strchr(cp, '/'))
			*cp = '\0';

	if (NULL == parse.article->title) {
		assert(NULL == parse.article->titletext);
		parse.article->title = 
			xstrdup("Untitled article");
		parse.article->titlesz = 
			strlen(parse.article->title);
		parse.article->titletext = 
			xstrdup("Untitled article");
		parse.article->titletextsz = 
			strlen(parse.article->titletext);
	}
	if (NULL == parse.article->author) {
		assert(NULL == parse.article->authortext);
		parse.article->author = 
			xstrdup("Untitled author");
		parse.article->authorsz = 
			strlen(parse.article->author);
		parse.article->authortext = 
			xstrdup("Untitled author");
		parse.article->authortextsz = 
			strlen(parse.article->authortext);
	}
	if (0 == parse.article->time) {
		if (-1 == fstat(fd, &st)) {
			perror(src);
			goto out;
		}
		parse.article->time = st.st_ctime;
	}
	if (NULL == parse.article->aside) {
		assert(NULL == parse.article->asidetext);
		parse.article->aside = xstrdup("");
		parse.article->asidetext = xstrdup("");
		parse.article->asidesz =
			parse.article->asidetextsz = 0;
	}

	rc = 1;
out:
	mmap_close(fd, buf, sz);
	return(rc);
}
