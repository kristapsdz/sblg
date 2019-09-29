/*	$Id$ */
/*
 * Copyright (c) 2013--2019 Kristaps Dzonsons <kristaps@bsd.lv>,
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
#if HAVE_ERR
# include <err.h>
#endif
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
	size_t		  spos; /* current sarg being shown */ 
	size_t		  sposz; /* size of sargs */
	size_t		  ssposz;  /* number of sargs to show */
	size_t		  stack; /* temporary: tag stack size */
	size_t		  navstart; /* temporary: nav items to show */
	size_t		  navlen; /* temporary: nav items to show */
	char		**navtags; /* list of navtags to query */
	size_t		  navtagsz; /* size of navtags list */
	int		  navuse; /* use navigation contents */
	enum asort	  navsort; /* override sort order */
	int		  usesort; /* whether to use navsort */
	int		  navxml; /* don't print html elements */
	ssize_t		  single; /* page index in -C/-L mode */
	char		 *nav; /* temporary: nav buffer */
	size_t		  navsz; /* nav buffer length */
	char		 *buf; /* buffer for text */
	size_t		  bufsz; /* buffer size */
};

static void tmpl_begin(void *, const XML_Char *, const XML_Char **);

static void
tmpl_text(void *dat, const XML_Char *s, int len)
{
	struct linkall	*arg = dat;

	if (arg->single != -1)
		xmlstrtext(&arg->buf, &arg->bufsz, s, len);
	else
		fprintf(arg->f, "%.*s", len, s);
}

static void
tmpl_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;

	if (arg->single != -1) {
		xmltextx(arg->f, arg->buf, arg->dst, 
			arg->sargs, arg->sposz, arg->sposz, 
			arg->single, arg->single, arg->sposz, 
			XMLESC_NONE);
		free(arg->buf);
		arg->buf = NULL;
		arg->bufsz = 0;
	}

	xmlclose(arg->f, s);
}

static void
article_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct linkall	*arg = dat;

	arg->stack += (sblg_lookup(s) == SBLG_ELEM_ARTICLE);
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

	arg->stack += (sblg_lookup(s) == SBLG_ELEM_NAV);
	xmlstropen(&arg->nav, &arg->navsz, s, atts, NULL);
}

/*
 * Find at least one of the given "tags" in "tagmap".
 * If "tags" is NULL or the tag was found, return 1.
 * If "tagmap" is empty or the tag wasn't found, return 0.
 */
static int
tagfind(char **tags, size_t tagsz, char **tagmap, size_t tagmapsz)
{
	size_t	 	 i, j;

	if (tagsz == 0)
		return 1;
	if (tagmapsz == 0) 
		return 0;

	for (i = 0; i < tagsz; i++) 
		for (j = 0; j < tagmapsz; j++)
			if (0 == strcmp(tags[i], tagmap[j]))
				return 1;

	return 0;
}

static void
nav_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;
	size_t		 i, k, count, setsz;
	char		 buf[32]; 
	int		 rc;
	struct article	*sv = NULL;

	if (sblg_lookup(s) != SBLG_ELEM_NAV || --arg->stack != 0) {
		xmlstrclose(&arg->nav, &arg->navsz, s);
		return;
	}

	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	XML_SetDefaultHandlerExpand(arg->p, tmpl_text);

	/* Only open the <ul> if we're printing HTML content. */

	if (!arg->navxml) {
		fputc('\n', arg->f);
		xmlopen(arg->f, "ul", NULL);
		fputc('\n', arg->f);
	}

	if (arg->usesort) {
		sv = arg->sargs;
		arg->sargs = calloc(arg->sposz, sizeof(struct article));
		memcpy(arg->sargs, sv, arg->sposz * sizeof(struct article));
		sblg_sort(arg->sargs, arg->sposz, arg->navsort);
	}

	/*
	 * Advance until "k" is at the article we want to start
	 * printing.
	 * This accounts for the starting article to show; which, due to
	 * tagging, might not be a true offset.
	 */

	for (i = k = 0; i < arg->navstart && k < arg->sposz; k++) {
		rc = tagfind(arg->navtags, arg->navtagsz, 
			arg->sargs[k].tagmap, arg->sargs[k].tagmapsz);
		i += (rc != 0);
	}

	/* Count total number of remaining articles. */

	for (i = k, setsz = count = 0; i < arg->sposz; i++) {
		rc = tagfind(arg->navtags, arg->navtagsz, 
			arg->sargs[i].tagmap, arg->sargs[i].tagmapsz);
		if (rc == 0)
			continue;
		if (count < arg->navlen)
			count++;
		setsz++;
	}

	/*
	 * Start showing articles from the first one, above.
	 * If we haven't been provided a navigation template (i.e., what
	 * was within the navigation tags), then make a simple default
	 * consisting of a list entry.
	 */

	for (i = 0; k < arg->sposz; k++) {
		rc = tagfind(arg->navtags, arg->navtagsz, 
			arg->sargs[k].tagmap, arg->sargs[k].tagmapsz);
		if (rc == 0)
			continue;

		if (arg->navxml) {
			xmltextx(arg->f, arg->nav, arg->dst,
				arg->sargs, setsz, arg->sposz, k, i, 
				count, XMLESC_NONE);
		} else if (!arg->navuse || arg->navsz == 0) {
			(void)strftime(buf, sizeof(buf), "%Y-%m-%d", 
				gmtime(&arg->sargs[k].time));
			xmlopen(arg->f, "li", NULL);
			fputs(buf, arg->f);
			fputs(": ", arg->f);
			xmlopen(arg->f, "a", "href", 
				arg->sargs[k].src, NULL);
			fputs(arg->sargs[k].titletext, arg->f);
			xmlclose(arg->f, "a");
			xmlclose(arg->f, "li");
			fputc('\n', arg->f);
		} else {
			xmlopen(arg->f, "li", NULL);
			xmltextx(arg->f, arg->nav, arg->dst, 
				arg->sargs, setsz, arg->sposz, k, i, 
				count, XMLESC_NONE);
			xmlclose(arg->f, "li");
		}

		if (++i >= arg->navlen)
			break;
	}

	if (!arg->navxml) {
		xmlclose(arg->f, "ul");
		xmlclose(arg->f, s);
	}

	free(arg->nav);
	arg->nav = NULL;
	arg->navsz = 0;

	for (i = 0; i < arg->navtagsz; i++)
		free(arg->navtags[i]);

	free(arg->navtags);
	arg->navtags = NULL;
	arg->navtagsz = 0;

	if (sv != NULL) {
		free(arg->sargs);
		arg->sargs = sv;
	}
}

static void
article_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;

	if (sblg_lookup(s) == SBLG_ELEM_ARTICLE && --arg->stack == 0) {
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
	}
}

static void
tmpl_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct linkall	 *arg = dat;
	const XML_Char	**attp;
	const XML_Char	 *sort = NULL;
	char		**tags = NULL;
	size_t		  i, tagsz = 0;

	assert(arg->stack == 0);

	if (arg->single != -1) {
		xmltextx(arg->f, arg->buf, arg->dst, 
			arg->sargs, arg->sposz, arg->sposz, 
			arg->single, arg->single, arg->sposz, 
			XMLESC_NONE);
		free(arg->buf);
		arg->buf = NULL;
		arg->bufsz = 0;
	}

	switch (sblg_lookup(s)) {
	case SBLG_ELEM_NAV:
		for (attp = atts; *attp != NULL; attp += 2) 
			if (sblg_lookup(*attp) == SBLG_ATTR_NAV)
				break;

		if (*attp == NULL || !xmlbool(attp[1])) {
			xmlopens(arg->f, s, atts);
			return;
		}

		arg->navuse = 0;
		arg->navsort = ASORT_DATE;
		arg->usesort = 0;
		arg->navxml = 0;
		arg->navlen = arg->sposz;
		arg->navstart = 0;

		for (attp = atts; *attp != NULL; attp += 2)
			switch (sblg_lookup(*attp)) {
			case SBLG_ATTR_NAVSZ:
				arg->navlen = atoi(attp[1]);
				if (arg->navlen > (size_t)arg->sposz)
					arg->navlen = arg->sposz;
				break;
			case SBLG_ATTR_NAVSTART:
				arg->navstart = atoi(attp[1]);
				if (arg->navstart > (size_t)arg->sposz)
					arg->navstart = arg->sposz;
				if (arg->navstart)
					arg->navstart--;
				break;
			case SBLG_ATTR_NAVCONTENT:
				arg->navuse = xmlbool(attp[1]);
				break;
			case SBLG_ATTR_NAVXML:
				arg->navxml = xmlbool(attp[1]);
				break;
			case SBLG_ATTR_NAVTAG:
				hashtag(&arg->navtags, 
					&arg->navtagsz, attp[1],
					arg->sargs, arg->sposz, arg->single);
				break;
			case SBLG_ATTR_NAVSORT:
				sort = attp[1];
				break;
			default:
				break;
			}

		/* Are we overriding the sort order? */

		if (sort != NULL) {
			arg->usesort = 1;
			if (!sblg_sort_lookup(sort, &arg->navsort))
				arg->usesort = 0;
		}

		if (!arg->navxml)
			xmlopens(arg->f, s, atts);

		arg->stack++;
		XML_SetElementHandler(arg->p, nav_begin, nav_end);
		XML_SetDefaultHandlerExpand(arg->p, nav_text);
		return;
	case SBLG_ELEM_ARTICLE:
		break;
	default:
		if (arg->single != -1)
			xmlopensx(arg->f, s, atts, arg->dst,
				arg->sargs, arg->sposz, arg->single);
		else
			xmlopens(arg->f, s, atts);
		return;
	}

	/* Check for <article data-sblg-article>. */

	for (attp = atts; *attp != NULL; attp += 2) 
		if (sblg_lookup(*attp) == SBLG_ATTR_ARTICLE)
			break;

	if (*attp == NULL || !xmlbool(attp[1])) {
		xmlopens(arg->f, s, atts);
		return;
	}

	/*
	 * If we have data-sblg-ign-once, then ignore the current
	 * invocation and remove the data-sblg-ign-once.
	 */

	for (attp = atts; *attp != NULL; attp += 2) 
		if (sblg_lookup(*attp) == SBLG_ATTR_IGN_ONCE &&
		    xmlbool(attp[1])) {
			xmlopens(arg->f, s, atts);
			return;
		}

	/*
	 * See if we should only output certain tags.
	 * To accomplish this, we first parse the requested tags in
	 * "articletag" into an array of tags.
	 * This attribute may happen multiple times.
	 */

	for (attp = atts; *attp != NULL; attp += 2)
		if (sblg_lookup(*attp) == SBLG_ATTR_ARTICLETAG)
			hashtag(&tags, &tagsz, attp[1],
				arg->sargs, arg->sposz, arg->single);

	/* Look for the next article mathing the given tag. */

	for ( ; arg->spos < arg->ssposz; arg->spos++)
		if (tagfind(tags, tagsz, 
		    arg->sargs[arg->spos].tagmap,
		    arg->sargs[arg->spos].tagmapsz))
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
		XML_SetElementHandler(arg->p, article_begin, article_end);
		return;
	}

	/* First throw away children, then push out the article. */

	arg->stack++;
	XML_SetDefaultHandlerExpand(arg->p, NULL);
	XML_SetElementHandler(arg->p, article_begin, article_end);

	/* Echo the formatted text of the article. */

	if (arg->single != -1)
		xmltextx(arg->f, arg->sargs[arg->spos].article, 
			arg->dst, arg->sargs, arg->sposz, 
			arg->sposz, arg->spos, arg->single, 
			arg->sposz, XMLESC_NONE);
	else
		xmltextx(arg->f, arg->sargs[arg->spos].article, 
			arg->dst, arg->sargs, arg->sposz, arg->sposz,
			arg->spos, 0, 1, XMLESC_NONE);
	arg->spos++;

	for (attp = atts; *attp != NULL; attp += 2) 
		if (sblg_lookup(*attp) == SBLG_ATTR_PERMLINK &&
		    xmlbool(attp[1])) {
			xmlopen(arg->f, "div", 
				"data-sblg-permlink", "1", NULL);
			xmlopen(arg->f, "a", "href", 
				arg->sargs[arg->spos - 1].src, NULL);
			fputs("permanent link", arg->f);
			xmlclose(arg->f, "a");
			xmlclose(arg->f, "div");
			fputc('\n', arg->f);
		}
}

/*
 * Given a set of articles "src", grok articles from the files, then
 * fill in a template that's usually the blog "front page".
 * If "force" is specified, use only that page instead of using all of
 * them (as in -C mode).
 * Return zero on fatal error, non-zero on success.
 */
int
linkall(XML_Parser p, const char *templ, const char *force, 
	int sz, char *src[], const char *dst, enum asort asort)
{
	char		*buf = NULL;
	size_t		 j, ssz = 0;
	int		 i, fd = -1, rc = 0;
	FILE		*f = stdout;
	struct linkall	 larg;
	struct article	*sargs = NULL;
	size_t		 sargsz = 0;

	memset(&larg, 0, sizeof(struct linkall));

	/* Grok all article data and sort by date. */

	for (i = 0; i < sz; i++)
		if (!sblg_parse(p, src[i], &sargs, &sargsz, NULL))
			goto out;

	sblg_sort(sargs, sargsz, asort);

	/* Open a FILE to the output file or stream. */

	if (strcmp(dst, "-") && ((f = fopen(dst, "w"))) == NULL) {
		warn("%s", dst);
		goto out;
	} 
	
	/* Map the template into memory for parsing. */

	if (!mmap_open(templ, &fd, &buf, &ssz))
		goto out;

	/*
	 * By default, we want to show all the articles we have in our
	 * input; however, if we're going to force a single entry to be
	 * shown, then find it in our arguments.
	 */

	larg.sargs = sargs;
	larg.sposz = larg.ssposz = sargsz;
	larg.p = p;
	larg.src = templ;
	larg.dst = strcmp(dst, "-") ? dst : NULL;
	larg.f = f;
	larg.single = -1;

	if (force != NULL) {
		for (j = 0; j < sargsz; j++)
			if (strcmp(force, sargs[j].src) == 0)
				break;
		if (j < sargsz) {
			larg.single = j;
			larg.spos = j;
			larg.ssposz = j + 1;
		} else {
			warnx("%s: not in input list", force);
			goto out;
		}
	}

	/* Run the XML parser on the template. */

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, tmpl_text);
	XML_SetElementHandler(p, tmpl_begin, tmpl_end);
	XML_SetUserData(p, &larg);

	if (XML_Parse(p, buf, (int)ssz, 1) != XML_STATUS_OK) {
		warnx("%s:%zu:%zu: %s", templ, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	fputc('\n', f);
	rc = 1;
out:
	sblg_free(sargs, sargsz);
	mmap_close(fd, buf, ssz);
	if (f != NULL && f != stdout)
		fclose(f);
	for (j = 0; j < larg.navtagsz; j++)
		free(larg.navtags[j]);
	free(larg.navtags);
	free(larg.nav);
	free(larg.buf);
	return rc;
}

/*
 * Like linkall() but does the output in place: groks all input files,
 * then converts them to output.
 * This prevents needing to run -C with each input file.
 * Return zero on fatal error, non-zero on success.
 */
int
linkall_r(XML_Parser p, const char *templ, 
	int sz, char *src[], enum asort asort)
{
	char		*buf = NULL, *dst = NULL;
	size_t		 j, ssz = 0, wsz;
	int		 i, fd = -1, rc = 0;
	FILE		*f = NULL;
	struct linkall	 larg;
	struct article	*sargs = NULL;
	size_t		 sargsz = 0;
	const char	*cp;

	memset(&larg, 0, sizeof(struct linkall));

	/* 
	 * Grok all article data then sort.
	 * Ignore cmdline sort order: it's already like that.
	 */

	for (i = 0; i < sz; i++)
		if (!sblg_parse(p, src[i], &sargs, &sargsz, NULL))
			goto out;

	sblg_sort(sargs, sargsz, asort);

	/* Map the template into memory for parsing. */

	if (!mmap_open(templ, &fd, &buf, &ssz))
		goto out;

	/*
	 * Iterate through each input article.
	 * Replace its filename with HTML and use that as the output.
	 */

	for (j = 0; j < sargsz; j++) {
		wsz = strlen(sargs[j].src);
		if ((cp = strrchr(sargs[j].src, '.')) == NULL || 
		    strcasecmp(cp + 1, "xml")) {
			/* Append .html to input name. */
			dst = xmalloc(wsz + 6);
			strlcpy(dst, sargs[j].src, wsz + 6);
			strlcat(dst, ".html", wsz + 6);
		} else {
			/* Replace .xml with .html. */
			dst = xmalloc(wsz + 2);
			strlcpy(dst, sargs[j].src, wsz - 2);
			strlcat(dst, "html", wsz + 2);
		} 

		/* Open the output filename. */
		
		if ((f = fopen(dst, "w")) == NULL) {
			warn("%s", dst);
			goto out;
		} 

		larg.sargs = sargs;
		larg.sposz = sargsz;
		larg.p = p;
		larg.src = templ;
		larg.dst = dst;
		larg.f = f;
		larg.single = j;
		larg.spos = j;
		larg.ssposz = j + 1;

		/* Run the XML parser on the template. */

		XML_ParserReset(p, NULL);
		XML_SetDefaultHandlerExpand(p, tmpl_text);
		XML_SetElementHandler(p, tmpl_begin, tmpl_end);
		XML_SetUserData(p, &larg);

		if (XML_Parse(p, buf, (int)ssz, 1) != XML_STATUS_OK) {
			warnx("%s:%zu:%zu: %s", templ, 
				XML_GetCurrentLineNumber(p),
				XML_GetCurrentColumnNumber(p),
				XML_ErrorString(XML_GetErrorCode(p)));
			goto out;
		} 

		fputc('\n', f);
		fclose(f);
		f = NULL;
		free(dst);
		dst = NULL;
	}
	rc = 1;

out:
	sblg_free(sargs, sargsz);
	mmap_close(fd, buf, ssz);
	if (f != NULL)
		fclose(f);
	for (j = 0; j < larg.navtagsz; j++)
		free(larg.navtags[j]);
	free(larg.navtags);
	free(larg.nav);
	free(larg.buf);
	free(dst);
	return rc;
}
