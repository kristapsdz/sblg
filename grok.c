/*	$Id$ */
/*
 * Copyright (c) 2013, 2014, 2017, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <ctype.h>
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
	struct article	**articles; /* vector of articles */
	size_t		 *articlesz; /* number of articles */
	size_t		  stack; /* stack (many uses) */
	size_t		  gstack; /* global "article" stack */
#define	PARSE_ASIDE	  1 /* we've seen an aside */
#define	PARSE_TIME	  2 /* we've seen a time */
#define	PARSE_ADDR	  4 /* we've seen an address (author) */
#define	PARSE_TITLE	  8 /* we've seen a title */
#define	PARSE_IMG	  16 /* we've seen an image */
	unsigned int	  flags;
	int		  fd; /* underlying descriptor */
	const char	 *src; /* underlying file */
	const char	**wl; /* whitelist of attributes */
};

static void article_begin(void *, const XML_Char *, const XML_Char **);
static void article_end(void *, const XML_Char *);
static void input_begin(void *, const XML_Char *, const XML_Char **);
static void logerrx(const struct parse *, const char *, ...)
		__attribute__((format(printf, 2, 3)));

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

/*
 * Parse the string into an RFC 3339 date/time.
 * Accepts YYYY-MM-DD or YYYY-MM-DDTHH:MM:SSZ.
 * Returns the pointer on success or NULL on failure.
 */
static struct tm *
string2tm(const char *cp, struct tm *tm, int *datetime)
{
	size_t	 sz;
	char	*erp;

	while (isspace((unsigned char)*cp))
		cp++;
	sz = strlen(cp);
	memset(tm, 0, sizeof(struct tm));
	if (sz == 10) {
		erp = strptime(cp, "%Y-%m-%d", tm);
		if (erp == NULL|| *erp != '\0')
			return NULL;
		*datetime = 0;
	} else if (sz == 20) {
		erp = strptime(cp, "%Y-%m-%dT%TZ", tm);
		if (erp == NULL || *erp != '\0')
			return NULL;
		*datetime = 1;
	} else
		return NULL;

	return tm;
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

	switch (sblg_lookup(s)) {
	case SBLG_ELEM_H1:
	case SBLG_ELEM_H2:
	case SBLG_ELEM_H3:
	case SBLG_ELEM_H4:
		XML_SetElementHandler(arg->p, 
			article_begin, article_end);
		XML_SetDefaultHandlerExpand(arg->p, article_text);
		break;
	default:
		xmlstrclose(&arg->article->title, 
			&arg->article->titlesz, s);
	}
}

static void
aside_end(void *dat, const XML_Char *s)
{
	struct parse	*arg = dat;

	xmlstrclose(&arg->article->article,
		&arg->article->articlesz, s);

	if (sblg_lookup(s) == SBLG_ELEM_ASIDE && --arg->stack == 0) {
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

	if (sblg_lookup(s) == SBLG_ELEM_ADDRESS && --arg->stack == 0) {
		XML_SetElementHandler(arg->p, 
			article_begin, article_end);
		XML_SetDefaultHandlerExpand(arg->p, article_text);
	} else
		xmlstrclose(&arg->article->author, 
			&arg->article->authorsz, s);
}

static void
thash(struct parse *arg, const char *key, const char *val)
{
	size_t	 i;

	assert(key != NULL && *key != '\0');

	for (i = 0; i < arg->article->setmapsz; i += 2)
		if (strcmp(key, arg->article->setmap[i]) == 0) {
			free(arg->article->setmap[i + 1]);
			arg->article->setmap[i + 1] = xstrdup(val);
			return;
		}

	arg->article->setmap = xreallocarray
		(arg->article->setmap,
		 arg->article->setmapsz + 2, sizeof(char *));
	arg->article->setmap[arg->article->setmapsz] = xstrdup(key);
	arg->article->setmap[arg->article->setmapsz + 1] = xstrdup(val);
	arg->article->setmapsz += 2;
}

/*
 * Look for sblg attributes defined on any given element.
 * This should be run by each and every element within the article.
 */
static void
tsearch(struct parse *arg, const XML_Char *s, const XML_Char **atts)
{
	const XML_Char	**attp;
	struct tm	  tm;

	for (attp = atts; *attp != NULL; attp += 2)
		switch (sblg_lookup(*attp)) {
		case SBLG_ATTR_CONST_ASIDE:
			if ((arg->flags & PARSE_ASIDE))
				break;
			/* FALLTHROUGH */
		case SBLG_ATTR_ASIDE:
			free(arg->article->aside);
			free(arg->article->asidetext);
			arg->article->aside = xstrdup(attp[1]);
			arg->article->asidetext = xstrdup(attp[1]);
			arg->article->asidetextsz = 
				arg->article->asidesz =
				strlen(arg->article->asidetext);
			arg->flags |= PARSE_ASIDE;
			break;
		case SBLG_ATTR_CONST_AUTHOR:
			if ((arg->flags & PARSE_ADDR))
				break;
			/* FALLTHROUGH */
		case SBLG_ATTR_AUTHOR:
			free(arg->article->author);
			free(arg->article->authortext);
			arg->article->author = xstrdup(attp[1]);
			arg->article->authortext = xstrdup(attp[1]);
			arg->article->authortextsz = 
				arg->article->authorsz =
				strlen(arg->article->author);
			arg->flags |= PARSE_ADDR;
			break;
		case SBLG_ATTR_CONST_IMG:
			if ((arg->flags & PARSE_IMG))
				break;
			/* FALLTHROUGH */
		case SBLG_ATTR_IMG:
			free(arg->article->img);
			arg->article->img = xstrdup(attp[1]);
			arg->flags |= PARSE_IMG;
			break;
		case SBLG_ATTR_TAGS:
			hashtag(&arg->article->tagmap,
				&arg->article->tagmapsz, attp[1],
				NULL, 0, 0);
			break;
		case SBLG_ATTR_CONST_TITLE:
			if ((arg->flags & PARSE_TITLE))
				break;
			/* FALLTHROUGH */
		case SBLG_ATTR_TITLE:
			free(arg->article->title);
			free(arg->article->titletext);
			arg->article->title = xstrdup(attp[1]);
			arg->article->titletext = xstrdup(attp[1]);
			arg->article->titletextsz = 
				arg->article->titlesz =
				strlen(arg->article->titletext);
			arg->flags |= PARSE_TITLE;
			break;
		case SBLG_ATTR_CONST_DATETIME:
			if ((arg->flags & PARSE_TIME))
				break;
			/* FALLTHROUGH */
		case SBLG_ATTR_DATETIME:
			if (string2tm(attp[1], &tm, 
			    &arg->article->isdatetime) == NULL) {
				logerrx(arg, "malformed "
					"RFC 3339 date/time");
				break;
			}
			arg->flags |= PARSE_TIME;
			arg->article->time = timegm(&tm);
			break;
		case SBLG_ATTR_SOURCE:
			free(arg->article->src);
			arg->article->src = xstrdup(attp[1]);
			break;
		default:
			if (strncasecmp(*attp, "data-sblg-set-", 14))
				break;
			if ((*attp)[14] != '\0')
				thash(arg, *attp + 14, attp[1]);
			break;
		}
}

static void
title_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	*arg = dat;

	arg->stack += (sblg_lookup(s) == SBLG_ELEM_TITLE);
	xmlstropen(&arg->article->title, 
		&arg->article->titlesz, s, atts, arg->wl);
	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts, arg->wl);
	tsearch(arg, s, atts);
}


static void
addr_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	*arg = dat;

	arg->stack += (sblg_lookup(s) == SBLG_ELEM_ADDRESS);
	xmlstropen(&arg->article->author, 
		&arg->article->authorsz, s, atts, arg->wl);
	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts, arg->wl);
	tsearch(arg, s, atts);
}

static void
aside_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	*arg = dat;

	arg->stack += (sblg_lookup(s) == SBLG_ELEM_ASIDE);
	xmlstropen(&arg->article->aside, 
		&arg->article->asidesz, s, atts, arg->wl);
	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts, arg->wl);
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

	assert(arg->stack == 0);

	xmlstropen(&arg->article->article,
		&arg->article->articlesz, s, atts, arg->wl);
	tsearch(arg, s, atts);

	switch (sblg_lookup(s)) {
	case SBLG_ELEM_ASIDE:
		if ((arg->flags & PARSE_ASIDE))
			return;
		arg->stack++;
		arg->flags |= PARSE_ASIDE;
		XML_SetDefaultHandlerExpand(arg->p, aside_text);
		XML_SetElementHandler(arg->p, aside_begin, aside_end);
		break;
	case SBLG_ELEM_IMG:
		if ((arg->flags & PARSE_IMG))
			return;
		for (attp = atts; *attp != NULL; attp += 2)
			if (strcmp(*attp, "src") == 0)
				break;
		if (attp[0] != NULL && attp[1] != NULL) {
			arg->article->img = xstrdup(attp[1]);
			arg->flags |= PARSE_IMG;
		}
		break;
	case SBLG_ELEM_TIME:
		if ((arg->flags & PARSE_TIME))
			return;
		arg->flags |= PARSE_TIME;
		for (attp = atts; *attp != NULL; attp += 2) {
			if (strcmp(*attp, "datetime"))
				continue;
			if (string2tm(attp[1], &tm, 
			    &arg->article->isdatetime) == NULL) {
				logerrx(arg, "malformed "
					"RFC 3339 date/time");
				continue;
			}
			arg->article->time = timegm(&tm);
		}
		break;
	case SBLG_ELEM_ADDRESS:
		if ((arg->flags & PARSE_ADDR) )
			return;
		arg->flags |= PARSE_ADDR;
		assert(arg->stack == 0);
		arg->stack++;
		XML_SetElementHandler(arg->p, addr_begin, addr_end);
		XML_SetDefaultHandlerExpand(arg->p, addr_text);
		break;
	case SBLG_ELEM_H1:
	case SBLG_ELEM_H2:
	case SBLG_ELEM_H3:
	case SBLG_ELEM_H4:
		if ((arg->flags & PARSE_TITLE))
			return;
		arg->flags |= PARSE_TITLE;
		XML_SetElementHandler(arg->p, title_begin, title_end);
		XML_SetDefaultHandlerExpand(arg->p, title_text);
		break;
	case SBLG_ELEM_ARTICLE:
		arg->gstack++;
		break;
	default:
		break;
	}
}

static void
article_end(void *dat, const XML_Char *s)
{
	struct parse	*arg = dat;
	char		*cp;
	struct stat	 st;

	xmlstrclose(&arg->article->article,
		&arg->article->articlesz, s);

	if (sblg_lookup(s) != SBLG_ELEM_ARTICLE || --arg->gstack > 0) 
		return;

	XML_SetElementHandler(arg->p, input_begin, NULL);
	XML_SetDefaultHandlerExpand(arg->p, NULL);

	/* Set source to "real" by default. */

	if (arg->article->src == NULL)
		arg->article->src = xstrdup(arg->src);

	/* Configure the "base" value and its derivatives. */

	arg->article->base = xstrdup(arg->article->src);

	if ((cp = strrchr(arg->article->src, '/')) == NULL) {
		arg->article->stripbase = xstrdup(arg->article->src);
		arg->article->stripsrc = xstrdup(arg->article->src);
	} else {
		arg->article->stripbase = xstrdup(cp + 1);
		arg->article->stripsrc = xstrdup(cp + 1);
	}

	if ((cp = strrchr(arg->article->src, '/')) == NULL)
		arg->article->striplangbase = xstrdup(arg->article->src);
	else
		arg->article->striplangbase = xstrdup(cp + 1);

	if ((cp = strrchr(arg->article->base, '.')) != NULL)
		if (strchr(cp, '/') == NULL)
			*cp = '\0';
	if ((cp = strrchr(arg->article->stripbase, '.')) != NULL)
		if (strchr(cp, '/') == NULL)
			*cp = '\0';
	if ((cp = strrchr(arg->article->striplangbase, '.')) != NULL)
		if (strchr(cp, '/') == NULL)
			*cp = '\0';

	/* Configure the "real" value and its derivatives. */

	arg->article->real = xstrdup(arg->src);
	arg->article->realbase = xstrdup(arg->article->real);

	if ((cp = strrchr(arg->article->real, '/')) == NULL) {
		arg->article->striprealbase = xstrdup(arg->article->real);
		arg->article->stripreal = xstrdup(arg->article->real);
	} else {
		arg->article->striprealbase = xstrdup(cp + 1);
		arg->article->stripreal = xstrdup(cp + 1);
	}

	if ((cp = strrchr(arg->article->real, '/')) == NULL)
		arg->article->striplangrealbase = xstrdup(arg->article->real);
	else
		arg->article->striplangrealbase = xstrdup(cp + 1);

	if ((cp = strrchr(arg->article->realbase, '.')) != NULL)
		if (strchr(cp, '/') == NULL)
			*cp = '\0';
	if ((cp = strrchr(arg->article->striprealbase, '.')) != NULL)
		if (strchr(cp, '/') == NULL)
			*cp = '\0';
	if ((cp = strrchr(arg->article->striplangrealbase, '.')) != NULL)
		if (strchr(cp, '/') == NULL)
			*cp = '\0';

	/* Configure title. */

	if (arg->article->title == NULL) {
		assert(arg->article->titletext == NULL);
		arg->article->title = xstrdup("Untitled article");
		arg->article->titlesz = strlen(arg->article->title);
		arg->article->titletext = xstrdup("Untitled article");
		arg->article->titletextsz = 
			strlen(arg->article->titletext);
	}

	/* Configure author. */

	if (arg->article->author == NULL) {
		assert(arg->article->authortext == NULL);
		arg->article->author = xstrdup("Untitled author");
		arg->article->authorsz = strlen(arg->article->author);
		arg->article->authortext = xstrdup("Untitled author");
		arg->article->authortextsz = 
			strlen(arg->article->authortext);
	}

	/* Configure datetime. */

	if (arg->article->time == 0) {
		arg->article->isdatetime = 1;
		if (fstat(arg->fd, &st) == -1)
			warn("%s", arg->article->src);
		else
			arg->article->time = st.st_ctime;
	}

	/* Configure aside. */

	if (arg->article->aside == NULL) {
		assert(arg->article->asidetext == NULL);
		arg->article->aside = xstrdup("");
		arg->article->asidetext = xstrdup("");
		arg->article->asidesz = arg->article->asidetextsz = 0;
	}
}

/*
 * Check for the first instance of <article data-sblg-article>.
 * If found, initialise a new article for parsing.
 */
static void
input_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct parse	 *arg = dat;
	char		 *cp, *tok, *tofree, *loc, *start;
	char		  c;
	size_t		  sz;
	const XML_Char	**attp;

	assert(arg->gstack == 0);
	assert(arg->stack == 0);

	if (sblg_lookup(s) != SBLG_ELEM_ARTICLE)
		return;

	/* Look for the true-valued data-sblg-article.  */

	for (attp = atts; *attp != NULL; attp += 2)
		if (sblg_lookup(attp[0]) == SBLG_ATTR_ARTICLE)
			break;

	if (*attp == NULL || !xmlbool(attp[1]))
		return;

	/* We have an article: allocates its bits. */

	arg->flags = 0;
	arg->stack = 0;
	arg->gstack = 0;

	*arg->articles = xreallocarray
		(*arg->articles, 
		 *arg->articlesz + 1,
		 sizeof(struct article));
	arg->article = &(*arg->articles)[*arg->articlesz];
	(*arg->articlesz)++;
	memset(arg->article, 0, sizeof(struct article));

	arg->article->order = *arg->articlesz;

	for (attp = atts; *attp != NULL; attp += 2) 
		switch (sblg_lookup(*attp)) {
		case SBLG_ATTR_LANG:
			cp = tofree = xstrdup(attp[1]);
			while ((tok = strsep(&cp, " \t")) != NULL) {
				if (*tok == '\0')
					continue;
				start = arg->article->striplangbase;
				loc = strstr(start, tok);
				if (loc == NULL || loc == start)
					continue;
				if (loc[-1] != '.')
					continue;
				c = loc[strlen(tok)];
				if (c != '.' && c != '\0')
					continue;
				sz = strlen(loc) - strlen(tok) + 1;
				memmove(loc - 1, loc + strlen(tok), sz);
			}
			free(tofree);
			break;
		case SBLG_ATTR_SORT:
			if (strcmp(attp[1], "first") == 0)
				arg->article->sort = SORT_FIRST;
			else if (strcmp(attp[1], "last") == 0)
				arg->article->sort = SORT_LAST;
			break;
		default:
			break;
		}

	arg->gstack = 1;

	xmlstropen(&arg->article->article, 
		&arg->article->articlesz, s, atts, arg->wl);
	XML_SetElementHandler(arg->p, article_begin, article_end);
	XML_SetDefaultHandlerExpand(arg->p, article_text);

	tsearch(arg, s, atts);
}

/*
 * Main driver for parsing an article at file "src" into (if found) the
 * vector "arg" of current size "argsz".
 * If "wl" is specified, this is used as a white-list of element
 * attributes that we record when parsing into our buffers.
 * Returns zero on failure, non-zero on fatal error (file not found, map
 * failure, allocation error, parse error, etc.).
 */
int
sblg_parse(XML_Parser p, const char *src,
	struct article **arg, size_t *argsz, const char **wl)
{
	char		*buf;
	size_t		 sz;
	int		 fd;
	struct parse	 parse;
	enum XML_Status	 st;

	memset(&parse, 0, sizeof(struct parse));

	if (!mmap_open(src, &fd, &buf, &sz))
		return 0;

	parse.articles = arg;
	parse.articlesz = argsz;
	parse.src = src;
	parse.p = p;
	parse.fd = fd;
	parse.wl = wl;

	XML_ParserReset(p, NULL);
	XML_SetStartElementHandler(p, input_begin);
	XML_SetUserData(p, &parse);

	if ((st = XML_Parse(p, buf, (int)sz, 1)) != XML_STATUS_OK)
		logerr(&parse);

	mmap_close(fd, buf, sz);
	return (st == XML_STATUS_OK);
}
