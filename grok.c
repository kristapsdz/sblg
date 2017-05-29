/*	$Id$ */
/*
 * Copyright (c) 2013, 2014, 2017 Kristaps Dzonsons <kristaps@bsd.lv>,
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
#if HAVE_ERR
# include <err.h>
#endif
#include <expat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "extern.h"

struct	parse {
	XML_Parser	  p;
	struct article	 *article; /* article being parsed */
	struct article	**articles;
	size_t		 *articlesz;
	size_t		  stack; /* stack (many uses) */
	size_t		  gstack; /* global "article" stack */
#define	PARSE_ASIDE	  1 /* we've seen an aside */
#define	PARSE_TIME	  2 /* we've seen a time */
#define	PARSE_ADDR	  4 /* we've seen an address */
#define	PARSE_TITLE	  8 /* we've seen a title */
#define	PARSE_IMG	  16 /* we've seen an image */
	unsigned int	  flags;
	int		  fd; /* underlying descriptor */
	const char	 *src; /* underlying file */
};

/*
 * Forward declarations for circular references.
 */
static void	article_begin(void *dat, const XML_Char *s, 
			const XML_Char **atts);
static void	article_end(void *dat, const XML_Char *s);
static void	input_begin(void *, const XML_Char *, 
			const XML_Char **);

static void
logerrx(const struct parse *p, const char *fmt, ...)
{
	va_list	ap;
	char	buf[BUFSIZ];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	warnx("%s:%zu:%zu: %s", p->src, 
		XML_GetCurrentLineNumber(p->p),
		XML_GetCurrentColumnNumber(p->p), buf);
}

static void
logerr(const struct parse *p)
{

	logerrx(p, "%s", XML_ErrorString(XML_GetErrorCode(p->p)));
}

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

/*
 * Look for sblg attributes defined on any given element.
 * This should be run by each and every element within the article.
 */
static void
tsearch(struct parse *arg, const XML_Char *s, const XML_Char **atts)
{
	const XML_Char	**attp;

	for (attp = atts; NULL != *attp; attp += 2) {
		if ('\0' == attp[1][0])
			continue;
		if (0 == strcasecmp(*attp, "data-sblg-img")) {
			free(arg->article->img);
			arg->article->img = xstrdup(attp[1]);
			arg->flags |= PARSE_IMG;
		} else if (0 == strcasecmp(*attp, "data-sblg-tags"))
			hashtag(&arg->article->tagmap,
				&arg->article->tagmapsz, attp[1]);
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
	char		 *erp;
	size_t		  sz;

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
	} else if (0 == strcasecmp(s, "img")) {
		if (PARSE_IMG & arg->flags) 
			return;
		for (attp = atts; NULL != *attp; attp += 2)
			if (0 == strcmp(*attp, "src"))
				break;
		if (NULL != attp[0] && 
		    NULL != attp[1] && '\0' != *attp[1]) {
			arg->article->img = xstrdup(attp[1]);
			arg->flags |= PARSE_IMG;
		}
	} else if (0 == strcasecmp(s, "time")) {
		if (PARSE_TIME & arg->flags)
			return;
		arg->flags |= PARSE_TIME;
		for (attp = atts; NULL != *attp; attp += 2) {
			if (strcasecmp(attp[0], "datetime"))
				continue;
			memset(&tm, 0, sizeof(struct tm));
			sz = strlen(attp[1]);
			if (10 == sz) {
				erp = strptime(attp[1], "%F", &tm);
				if (NULL == erp || '\0' != *erp) {
					logerrx(arg, "malformed "
						"ISO 3339 date");
					continue;
				}
				arg->article->isdatetime = 0;
			} else if (20 == sz) {
				erp = strptime(attp[1], "%FT%TZ", &tm);
				if (NULL == erp || '\0' != *erp) {
					logerrx(arg, "malformed "
						"ISO 3339 datetime");
					continue;
				}
				arg->article->isdatetime = 1;
			} else {
				logerrx(arg, "malformed "
					"ISO 3339 datetime");
				continue;
			}
			arg->article->time = timegm(&tm);
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
	char		*cp;
	struct stat	 st;

	xmlstrclose(&arg->article->article,
		&arg->article->articlesz, s);

	if (strcasecmp(s, "article") || --arg->gstack > 0) 
		return;

	XML_SetElementHandler(arg->p, input_begin, NULL);
	XML_SetDefaultHandlerExpand(arg->p, NULL);

	if (NULL != (cp = strrchr(arg->article->base, '.')))
		if (NULL == strchr(cp, '/'))
			*cp = '\0';
	if (NULL != (cp = strrchr(arg->article->stripbase, '.')))
		if (NULL == strchr(cp, '/'))
			*cp = '\0';
	if (NULL != (cp = strrchr(arg->article->striplangbase, '.')))
		if (NULL == strchr(cp, '/'))
			*cp = '\0';

	if (NULL == arg->article->title) {
		assert(NULL == arg->article->titletext);
		arg->article->title = 
			xstrdup("Untitled article");
		arg->article->titlesz = 
			strlen(arg->article->title);
		arg->article->titletext = 
			xstrdup("Untitled article");
		arg->article->titletextsz = 
			strlen(arg->article->titletext);
	}
	if (NULL == arg->article->author) {
		assert(NULL == arg->article->authortext);
		arg->article->author = 
			xstrdup("Untitled author");
		arg->article->authorsz = 
			strlen(arg->article->author);
		arg->article->authortext = 
			xstrdup("Untitled author");
		arg->article->authortextsz = 
			strlen(arg->article->authortext);
	}
	if (0 == arg->article->time) {
		arg->article->isdatetime = 1;
		if (-1 == fstat(arg->fd, &st))
			warn("%s", arg->article->src);
		else
			arg->article->time = st.st_ctime;
	}
	if (NULL == arg->article->aside) {
		assert(NULL == arg->article->asidetext);
		arg->article->aside = xstrdup("");
		arg->article->asidetext = xstrdup("");
		arg->article->asidesz =
			arg->article->asidetextsz = 0;
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

	arg->flags = 0;
	arg->stack = 0;
	arg->gstack = 0;

	/* 
	 * We have an article.
	 * Allocates its bits.
	 */
	*arg->articles = xreallocarray
		(*arg->articles, 
		 *arg->articlesz + 1,
		 sizeof(struct article));
	arg->article = &(*arg->articles)[*arg->articlesz];
	(*arg->articlesz)++;
	memset(arg->article, 0, sizeof(struct article));

	arg->article->src = arg->src;
	arg->article->base = xstrdup(arg->src);
	if (NULL == strrchr(arg->src, '/'))
		arg->article->stripbase = xstrdup(arg->src);
	else
		arg->article->stripbase = xstrdup
			(strrchr(arg->src, '/') + 1);
	if (NULL == strrchr(arg->src, '/'))
		arg->article->striplangbase = xstrdup(arg->src);
	else
		arg->article->striplangbase = xstrdup
			(strrchr(arg->src, '/') + 1);

	/*
	 * If we have any languages specified, append them here.
	 */
	for (attp = atts; NULL != *attp; attp += 2) 
		if (0 == strcasecmp(*attp, "data-sblg-lang")) {
			cp = tofree = xstrdup(attp[1]);
			while ((tok = strsep(&cp, " \t")) != NULL) {
				if ('\0' == *tok)
					continue;
				start = arg->article->striplangbase;
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
		} else if (0 == strcasecmp(*attp, "data-sblg-sort")) {
			if (0 == strcasecmp(attp[1], "first"))
				arg->article->sort = SORT_FIRST;
			else if (0 == strcasecmp(attp[1], "last"))
				arg->article->sort = SORT_LAST;
		}

	arg->gstack = 1;
	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts);
	XML_SetElementHandler(arg->p, article_begin, article_end);
	XML_SetDefaultHandlerExpand(arg->p, article_text);
	tsearch(arg, s, atts);
}

int
sblg_parse(XML_Parser p, const char *src, struct article **arg, size_t *argsz)
{
	char		*buf;
	size_t		 sz;
	int		 fd, rc;
	struct parse	 parse;

	memset(&parse, 0, sizeof(struct parse));

	rc = 0;

	if ( ! mmap_open(src, &fd, &buf, &sz))
		goto out;

	parse.articles = arg;
	parse.articlesz = argsz;
	parse.src = src;
	parse.p = p;
	parse.fd = fd;

	XML_ParserReset(p, NULL);
	XML_SetStartElementHandler(p, input_begin);
	XML_SetUserData(p, &parse);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)sz, 1)) {
		logerr(&parse);
		goto out;
	} 

	rc = 1;
out:
	mmap_close(fd, buf, sz);
	return(rc);
}
