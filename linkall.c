/*	$Id$ */
/*
 * Copyright (c) 2013--2016 Kristaps Dzonsons <kristaps@bsd.lv>,
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

#include <assert.h>
#include <expat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

struct	linkall {
	FILE		 *f; /* open template file */
	const char	 *src; /* template file */
	const char	 *dst; /* output file (or empty)*/
	XML_Parser	  p; /* active parser */
	struct article	 *sargs; /* sorted article contents */
	int		  spos; /* current sarg being shown */ 
	int		  sposz; /* size of sargs */
	int		  ssposz;  /* number of sargs to show */
	size_t		  stack; /* temporary: tag stack size */
	size_t		  navlen; /* temporary: nav items to show */
	char		**navtags; /* list of navtags to query */
	size_t		  navtagsz; /* size of navtags list */
	int		  navuse; /* use navigation contents */
	ssize_t		  single; /* page index in -C mode*/
	char		 *nav; /* temporary: nav buffer */
	size_t		  navsz; /* nav buffer length */
	size_t		  navstart; /* nav buffer length */
	char		 *buf; /* buffer for text */
	size_t		  bufsz; /* buffer size */
	size_t		  bufmax; /* buffer maximum size */
};

static	void	tmpl_begin(void *dat, const XML_Char *s, 
			const XML_Char **atts);

static void
tmpl_text(void *dat, const XML_Char *s, int len)
{
	struct linkall	*arg = dat;

	if (-1 != arg->single)
		xmlstrtext(&arg->buf, &arg->bufsz, s, len);
	else
		fprintf(arg->f, "%.*s", len, s);
}

static void
tmpl_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;

	if (-1 != arg->single) {
		xmltextx(arg->f, arg->buf, arg->dst, 
			arg->sargs, arg->sposz, arg->single);
		xmlstrflush(arg->buf, &arg->bufsz);
	}

	xmlclose(arg->f, s);
}

static void
article_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct linkall	*arg = dat;

	arg->stack += 0 == strcasecmp(s, "article");
}

static void
nav_text(void *dat, const XML_Char *s, int len)
{
	struct linkall	*arg = dat;

	xmlstrtext(&arg->nav, &arg->navsz, s, len);
}

static void
nav_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct linkall	*arg = dat;

	arg->stack += 0 == strcasecmp(s, "nav");
	xmlstropen(&arg->nav, &arg->navsz, s, atts);
}

/*
 * Find the given tags of "arg" in the space-separated set of tags
 * "haystack".
 * If "arg" is NULL or the tag was found, return 1.
 * If "haystack" is NULL or the tag wasn't found, return 0.
 */
static int
tagfind(char **tags, size_t tagsz, const char *haystack)
{
	const char 	*cp, *needle;
	size_t	 	 i, sz;

	if (0 == tagsz)
		return(1);
	if (NULL == haystack) 
		return(0);

	for (i = 0; i < tagsz; i++) {
		needle = tags[i];
		sz = strlen(needle);
		for (;;) {
			if (NULL == (cp = strstr(haystack, needle)))
				break;
			if (cp > haystack && ' ' != cp[-1]) {
				haystack = cp + sz;
				continue;
			}
			cp += sz;
			if ('\0' == *cp || ' ' == *cp)
				return(1);
			haystack = cp;
		}
	}
	return(0);
}

static void
nav_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;
	size_t		 i;
	char		 buf[32]; 
	int		 k;

	if (strcasecmp(s, "nav") || 0 != --arg->stack) {
		xmlstrclose(&arg->nav, &arg->navsz, s);
		return;
	}

	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	XML_SetDefaultHandlerExpand(arg->p, tmpl_text);

	fputc('\n', arg->f);
	xmlopen(arg->f, "ul", NULL);
	fputc('\n', arg->f);

	/*
	 * If we haven't been provided a navigation template (i.e., what
	 * was within the navigation tags), then make a simple default
	 * consisting of a list entry.
	 */
	if ( ! arg->navuse || 0 == arg->navsz) {
		for (i = k = arg->navstart; k < arg->sposz; k++) {
			if ( ! tagfind(arg->navtags, arg->navtagsz, arg->sargs[k].tags))
				continue;
			(void)strftime(buf, sizeof(buf), "%F", 
				localtime(&arg->sargs[k].time));
			xmlopen(arg->f, "li", NULL);
			fputs(buf, arg->f);
			fputs(": ", arg->f);
			xmlopen(arg->f, "a", "href", 
				arg->sargs[k].src, NULL);
			fputs(arg->sargs[k].titletext, arg->f);
			xmlclose(arg->f, "a");
			xmlclose(arg->f, "li");
			fputc('\n', arg->f);
			if (i++ >= arg->navlen)
				break;
		}
	} else {
		/*
		 * We do have a navigation template.
		 * Output it, replacing key terms along the way.
		 */
		for (i = k = arg->navstart; k < arg->sposz; k++) {
			if ( ! tagfind(arg->navtags, arg->navtagsz, arg->sargs[k].tags))
				continue;
			xmlopen(arg->f, "li", NULL);
			xmltextx(arg->f, arg->nav, arg->dst, 
				arg->sargs, arg->sposz, k);
			xmlclose(arg->f, "li");
			if (++i >= arg->navlen)
				break;
		}
	}

	xmlclose(arg->f, "ul");
	xmlclose(arg->f, s);
	xmlstrflush(arg->nav, &arg->navsz);

	for (i = 0; i < arg->navtagsz; i++)
		free(arg->navtags[i]);

	free(arg->navtags);
	arg->navtags = NULL;
	arg->navtagsz = 0;
}

static void
empty_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;

	if (0 == strcasecmp(s, "article") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
	}
}

static void
article_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;

	if (0 == strcasecmp(s, "article") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
	}
}

static void
tagalloc(struct linkall *arg, const char *in)
{
	char	*token, *string, *sv;

	string = sv = xstrdup(in);
	while ((token = strsep(&string, " ")) != NULL) {
		if ('\0' == *token)
			continue;
		arg->navtags = xreallocarray(arg->navtags, 
			arg->navtagsz + 1, sizeof(char *));
		arg->navtags[arg->navtagsz++] = xstrdup(token);
	}
	free(sv);
}

static void
tmpl_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct linkall	 *arg = dat;
	const XML_Char	**attp;
	char		**tags;
	char		 *str, *tok, *tfr;
	size_t		  i, tagsz;

	assert(0 == arg->stack);

	if (-1 != arg->single) {
		xmltextx(arg->f, arg->buf, arg->dst, 
			arg->sargs, arg->sposz, arg->single);
		xmlstrflush(arg->buf, &arg->bufsz);
	}

	if (0 == strcasecmp(s, "nav")) {
		/*
		 * Only handle if containing the "data-sblg-nav"
		 * attribute, otherwise continue.
		 */
		xmlopens(arg->f, s, atts);
		for (attp = atts; NULL != *attp; attp += 2) 
			if (0 == strcasecmp(*attp, "data-sblg-nav"))
				break;

		if (NULL == *attp || ! xmlbool(attp[1]))
			return;

		/*
		 * Take the number of elements to show to be the min of
		 * the full count or as user-specified.
		 */
		arg->navuse = 0;
		arg->navlen = arg->sposz;
		arg->navstart = 0;
		for (attp = atts; NULL != *attp; attp += 2) {
			if (0 == strcasecmp(attp[0], 
					"data-sblg-navsz")) {
				arg->navlen = atoi(attp[1]);
				if (arg->navlen > (size_t)arg->sposz)
					arg->navlen = arg->sposz;
			/*} else if (0 == strcasecmp(attp[0],
					"data-sblg-navstart")) {
				arg->navstart = atoi(attp[1]);
				if (arg->navstart > (size_t)arg->sposz)
					arg->navstart = arg->sposz;*/
			} else if (0 == strcasecmp(attp[0], 
					"data-sblg-navcontent")) {
				arg->navuse = xmlbool(attp[1]);
			} else if (0 == strcasecmp(attp[0], 
					"data-sblg-navtag")) {
				tagalloc(arg, attp[1]);
			}
		}

		arg->stack++;
		XML_SetElementHandler(arg->p, nav_begin, nav_end);
		XML_SetDefaultHandlerExpand(arg->p, nav_text);
		return;
	} else if (strcasecmp(s, "article")) {
		if (-1 != arg->single)
			xmlopensx(arg->f, s, atts, arg->dst, 
				arg->sargs, arg->sposz, arg->single);
		else
			xmlopens(arg->f, s, atts);
		return;
	}

	/*
	 * Only consider article elements if they contain the magic
	 * data-sblg-article attribute.
	 */
	for (attp = atts; NULL != *attp; attp += 2) 
		if (0 == strcasecmp(*attp, "data-sblg-article"))
			break;

	if (NULL == *attp || ! xmlbool(attp[1])) {
		xmlopens(arg->f, s, atts);
		return;
	}

	/*
	 * See if we should only output certain tags.
	 * To accomplish this, we first parse the requested tags in
	 * "articletag" into an array of tags.
	 * This attribute may happen multiple times.
	 */
	tags = NULL;
	tagsz = 0;
	for (attp = atts; NULL != *attp; attp += 2) {
		if (strcasecmp(*attp, "data-sblg-articletag")) 
			continue;
		tfr = str = xstrdup(attp[1]);
		while (NULL != (tok = strsep(&str, " \t"))) {
			tags = xreallocarray(tags,
				tagsz + 1, sizeof(char *));
			tags[tagsz++] = xstrdup(tok);
		}
		free(tfr);
	}

	/* Look for the next article mathing the given tag. */
	for ( ; arg->spos < arg->ssposz; arg->spos++)
		if (tagfind(tags, tagsz, arg->sargs[arg->spos].tags))
			break;

	for (i = 0; i < tagsz; i++)
		free(tags[i]);
	free(tags);

	if (arg->spos >= arg->ssposz) {
		/*
		 * We have no articles left to show.
		 * Just continue throwing away this article element til
		 * we receive a matching one.
		 */
		arg->stack++;
		XML_SetDefaultHandlerExpand(arg->p, NULL);
		XML_SetElementHandler(arg->p, article_begin, empty_end);
		return;
	}

	/*
	 * First throw away children, then push out the article itself.
	 */
	arg->stack++;
	XML_SetDefaultHandlerExpand(arg->p, NULL);
	XML_SetElementHandler(arg->p, article_begin, article_end);

	/* Echo the formatted text of the article. */
	xmltextx(arg->f, arg->sargs[arg->spos].article, 
		arg->dst, arg->sargs, arg->sposz, arg->spos);
	arg->spos++;

	for (attp = atts; NULL != *attp; attp += 2) 
		if (0 == strcasecmp(*attp, "data-sblg-permlink"))
			break;
	if (NULL != *attp && ! xmlbool(attp[1]))
		return;

	xmlopen(arg->f, "div", "data-sblg-permlink", "1", NULL);
	xmlopen(arg->f, "a", "href", arg->sargs[arg->spos - 1].src, NULL);
	fputs("permanent link", arg->f);
	xmlclose(arg->f, "a");
	xmlclose(arg->f, "div");
	fputc('\n', arg->f);
}


/*
 * Given a set of articles "src", grok articles from the files, then
 * fill in a template that's usually the blog "front page".
 */
int
linkall(XML_Parser p, const char *templ, const char *force, 
	int sz, char *src[], const char *dst, enum asort asort)
{
	char		*buf;
	size_t		 j, ssz;
	int		 i, fd, rc;
	FILE		*f;
	struct linkall	 larg;
	struct article	*sarg;

	ssz = 0;
	rc = 0;
	buf = NULL;
	fd = -1;
	f = NULL;


	memset(&larg, 0, sizeof(struct linkall));
	sarg = xcalloc(sz, sizeof(struct article));

	/* Grok all article data and sort by date. */
	for (i = 0; i < sz; i++)
		if ( ! grok(p, src[i], &sarg[i]))
			goto out;

	if (ASORT_DATE == asort)
		qsort(sarg, sz, sizeof(struct article), datecmp);
	else 
		qsort(sarg, sz, sizeof(struct article), filenamecmp);

	/* Open a FILE to the output file or stream. */
	f = stdout;
	if (strcmp(dst, "-") && (NULL == (f = fopen(dst, "w")))) {
		perror(dst);
		goto out;
	} 
	
	/* Map the template into memory for parsing. */
	if ( ! mmap_open(templ, &fd, &buf, &ssz))
		goto out;

	/*
	 * By default, we want to show all the articles we have in our
	 * input; however, if we're going to force a single entry to be
	 * shown, then find it in our arguments.
	 */
	larg.sargs = sarg;
	larg.sposz = larg.ssposz = sz;
	larg.p = p;
	larg.src = templ;
	larg.dst = strcmp(dst, "-") ? dst : NULL;
	larg.f = f;
	larg.single = -1;

	if (NULL != force) {
		for (i = 0; i < sz; i++)
			if (0 == strcmp(force, sarg[i].src))
				break;
		if (i < sz) {
			larg.single = i;
			larg.spos = i;
			larg.ssposz = i + 1;
		} else {
			fprintf(stderr, "%s: does not "
				"appear in input list\n", force);
			goto out;
		}
	}

	/* Run the XML parser on the template. */
	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, tmpl_text);
	XML_SetElementHandler(p, tmpl_begin, tmpl_end);
	XML_SetUserData(p, &larg);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)ssz, 1)) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", templ, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	fputc('\n', f);
	rc = 1;
out:
	for (i = 0; i < sz; i++) 
		article_free(&sarg[i]);
	mmap_close(fd, buf, ssz);
	if (NULL != f && stdout != f)
		fclose(f);

	for (j = 0; j < larg.navtagsz; j++)
		free(larg.navtags[j]);
	free(larg.navtags);
	free(larg.nav);
	free(larg.buf);
	free(sarg);
	return(rc);
}

