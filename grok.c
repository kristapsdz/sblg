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
#define	PARSE_ADDR	  4 /* we've seen an address */
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

	if (strcasecmp(s, "h1") == 0 ||
	    strcasecmp(s, "h2") == 0 ||
	    strcasecmp(s, "h3") == 0 ||
	    strcasecmp(s, "h4") == 0) {
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

	if (strcasecmp(s, "aside") == 0 && --arg->stack == 0) {
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

	if (strcasecmp(s, "address") == 0 && --arg->stack == 0) {
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

	for (attp = atts; *attp != NULL; attp += 2)
		switch (sblg_lookup(*attp)) {
		case SBLGTAG_IMG:
			free(arg->article->img);
			arg->article->img = xstrdup(attp[1]);
			arg->flags |= PARSE_IMG;
			break;
		case SBLGTAG_TAGS:
			hashtag(&arg->article->tagmap,
				&arg->article->tagmapsz, attp[1],
				NULL, 0, 0);
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

	arg->stack += 0 == strcasecmp(s, "title");
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

	arg->stack += 0 == strcasecmp(s, "address");
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

	arg->stack += 0 == strcasecmp(s, "aside");
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
	const XML_Char	 *atcp;
	struct tm	  tm;
	char		 *erp;
	size_t		  sz;

	assert(arg->stack == 0);

	xmlstropen(&arg->article->article,
		&arg->article->articlesz, s, atts, arg->wl);
	tsearch(arg, s, atts);

	if (strcasecmp(s, "aside") == 0) {
		if (PARSE_ASIDE & arg->flags)
			return;
		arg->stack++;
		arg->flags |= PARSE_ASIDE;
		XML_SetDefaultHandlerExpand(arg->p, aside_text);
		XML_SetElementHandler(arg->p, aside_begin, aside_end);
	} else if (strcasecmp(s, "img") == 0) {
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
	} else if (strcasecmp(s, "time") == 0) {
		if (PARSE_TIME & arg->flags)
			return;
		arg->flags |= PARSE_TIME;
		for (attp = atts; NULL != *attp; attp += 2) {
			if (strcasecmp(attp[0], "datetime"))
				continue;
			atcp = attp[1];
			while (isspace((unsigned char)*atcp))
				atcp++;
			memset(&tm, 0, sizeof(struct tm));
			sz = strlen(atcp);
			if (10 == sz) {
				erp = strptime(atcp, "%Y-%m-%d", &tm);
				if (NULL == erp || '\0' != *erp) {
					logerrx(arg, "malformed "
						"ISO 3339 date");
					continue;
				}
				arg->article->isdatetime = 0;
			} else if (20 == sz) {
				erp = strptime(atcp, "%Y-%m-%dT%TZ", &tm);
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
	} else if (strcasecmp(s, "address") == 0) {
		if (PARSE_ADDR & arg->flags) 
			return;
		arg->flags |= PARSE_ADDR;
		assert(0 == arg->stack);
		arg->stack++;
		XML_SetElementHandler(arg->p, addr_begin, addr_end);
		XML_SetDefaultHandlerExpand(arg->p, addr_text);
	} else if (strcasecmp(s, "h1") == 0 ||
		   strcasecmp(s, "h2") == 0 ||
		   strcasecmp(s, "h3") == 0 ||
		   strcasecmp(s, "h4") == 0) {
		if (PARSE_TITLE & arg->flags) 
			return;
		arg->flags |= PARSE_TITLE;
		XML_SetElementHandler(arg->p, title_begin, title_end);
		XML_SetDefaultHandlerExpand(arg->p, title_text);
	} else if (strcasecmp(s, "article") == 0)
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

	if (strcasecmp(s, "article"))
		return;

	/* Look for the true-valued data-sblg-article.  */

	for (attp = atts; *attp != NULL; attp += 2)
		if (sblg_lookup(attp[0]) == SBLGTAG_ARTICLE)
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
	arg->article->src = arg->src;
	arg->article->base = xstrdup(arg->src);

	if ((cp = strrchr(arg->src, '/')) == NULL) {
		arg->article->stripbase = xstrdup(arg->src);
		arg->article->stripsrc = arg->src;
	} else {
		arg->article->stripbase = xstrdup(cp + 1);
		arg->article->stripsrc = cp + 1;
	}

	if ((cp = strrchr(arg->src, '/')) == NULL)
		arg->article->striplangbase = xstrdup(arg->src);
	else
		arg->article->striplangbase = xstrdup(cp + 1);

	for (attp = atts; *attp != NULL; attp += 2) 
		switch (sblg_lookup(*attp)) {
		case SBLGTAG_LANG:
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
		case SBLGTAG_SORT:
			if (strcasecmp(attp[1], "first") == 0)
				arg->article->sort = SORT_FIRST;
			else if (strcasecmp(attp[1], "last") == 0)
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
