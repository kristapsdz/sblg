/*
 * Copyright (c) Kristaps Dzonsons <kristaps@bsd.lv>,
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

enum	textmode {
	TEXT_NAV,
	TEXT_TMPL,
	TEXT_NONE,
};

/*
 * How the navigation tag (e.g., <nav data-sblg-nav="1">) is handled
 * when filling in navigation entries.
 */
enum	navelem {
	NAVELEM_KEEP_STRIP, /* keep element but strip attributes */
	NAVELEM_REPEAT_STRIP, /* repeat element per article but strip */
	NAVELEM_KEEP, /* keep element as-is */
	NAVELEM_DISCARD, /* discard element */
};

/*
 * How navigation entries are filled in to the navigation tag.
 */
enum	navformat {
	NAVFORMAT_KEEP, /* keep content and replace symbols */
	NAVFORMAT_SUMMARISE, /* summarise content */
	NAVFORMAT_LIST_SUMMARISE, /* summarise within ul/li */
	NAVFORMAT_LIST_KEEP, /* keep within ul/li */
};

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
	char		 *stacktag; /* tag starting nav/article or NULL */
	size_t		  navstart; /* temporary: nav items to show */
	size_t		  navlen; /* temporary: nav items to show */
	char		**navtags; /* list of navtags to query */
	size_t		  navtagsz; /* size of navtags list */
	enum asort	  navsort; /* override sort order */
	enum navelem	  navelem;
	enum navformat	  navformat;
	int		  usesort; /* whether to use navsort */
	ssize_t		  single; /* page index in -C/-L mode */
	char		 *nav; /* temporary: nav buffer */
	size_t		  navsz; /* nav buffer length */
	char		 *buf; /* buffer for text */
	size_t		  bufsz; /* buffer size */
	enum textmode	  textmode; /* mode to accept text */
};

static void tmpl_begin(void *, const XML_Char *, const XML_Char **);

static void
text(void *dat, const XML_Char *s, int len)
{
	struct linkall	*arg = dat;

	switch (arg->textmode) {
	case TEXT_TMPL:
		if (arg->single != -1)
			xmlstrtext(&arg->buf, &arg->bufsz, s, len);
		else
			fprintf(arg->f, "%.*s", len, s);
		break;
	case TEXT_NAV:
		xmlstrtext(&arg->nav, &arg->navsz, s, len);
		break;
	default:
		break;
	}
}

static void
entity(void *dat, const XML_Char *entity, int is_parameter_entity)
{
	/* Ignore this argument. */

	(void)is_parameter_entity; 

	/* Pass through as text. */

	text(dat, "&", 1);
	text(dat, entity, strlen(entity));
	text(dat, ";", 1);
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

	assert(arg->stacktag != NULL);
	arg->stack += strcmp(s, arg->stacktag) == 0;
}

/*
 * Called on content within a navigation block, which is usually invoked
 * as <nav data-sblg-nav="1">.  Keeps track of how many nested elements
 * of the same type as that which started the navigation block (usually
 * <nav>) are in play.  See nav_end() for when the navigation block
 * might end.
 */
static void
nav_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct linkall	*arg = dat;

	assert(arg->stacktag != NULL);
	arg->stack += strcmp(s, arg->stacktag) == 0;
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

/*
 * See if the navigation block should end, which happens when meeting a
 * non-nested close element of the same type which opened the navigation
 * block, usually <nav>.  See nav_begin().
 */
static void
nav_end(void *dat, const XML_Char *s)
{
	struct linkall	*arg = dat;
	size_t		 i, k, count, setsz;
	char		 buf[32]; 
	int		 rc;
	struct article	*sv = NULL;

	assert(arg->stacktag != NULL);
	if (strcmp(s, arg->stacktag) != 0 || --arg->stack != 0) {
		xmlstrclose(&arg->nav, &arg->navsz, s);
		return;
	}

	/* Closing out... */

	arg->textmode = TEXT_TMPL;
	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);

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

		if (arg->navformat == NAVFORMAT_LIST_SUMMARISE ||
		    arg->navformat == NAVFORMAT_LIST_KEEP)
			xmlopen(arg->f, "li", NULL);
		if (arg->navelem == NAVELEM_REPEAT_STRIP)
			xmlopen(arg->f, arg->stacktag, NULL);

		if (arg->navformat == NAVFORMAT_LIST_SUMMARISE ||
		    arg->navformat == NAVFORMAT_SUMMARISE) {
			(void)strftime(buf, sizeof(buf), "%Y-%m-%d", 
				gmtime(&arg->sargs[k].time));
			fputs(buf, arg->f);
			fputs(": ", arg->f);
			xmlopen(arg->f, "a", "href", 
				arg->sargs[k].src, NULL);
			fputs(arg->sargs[k].titletext, arg->f);
			xmlclose(arg->f, "a");
		} else
			xmltextx(arg->f, arg->nav, arg->dst,
				arg->sargs, setsz, arg->sposz, k, i, 
				count, XMLESC_NONE);

		if (arg->navelem == NAVELEM_REPEAT_STRIP)
			xmlclose(arg->f, arg->stacktag);
		if (arg->navformat == NAVFORMAT_LIST_SUMMARISE ||
		    arg->navformat == NAVFORMAT_LIST_KEEP)
			xmlclose(arg->f, "li");

		if (++i >= arg->navlen)
			break;
	}

	if (arg->navformat == NAVFORMAT_LIST_SUMMARISE ||
	    arg->navformat == NAVFORMAT_LIST_KEEP)
		xmlclose(arg->f, "ul");
	if (arg->navelem == NAVELEM_KEEP ||
	    arg->navelem == NAVELEM_KEEP_STRIP)
		xmlclose(arg->f, arg->stacktag);

	free(arg->stacktag);
	free(arg->nav);
	arg->stacktag = NULL;
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

	assert(arg->stacktag != NULL);
	if (strcmp(s, arg->stacktag) != 0 || --arg->stack != 0)
		return;

	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	arg->textmode = TEXT_TMPL;
	free(arg->stacktag);
	arg->stacktag = NULL;
}

static void
tmpl_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct linkall	 *arg = dat;
	const XML_Char	**attp;
	const XML_Char	 *sort = NULL;
	char		**tags = NULL;
	size_t		  i, tagsz = 0;
	int		  start_nav = 0, start_article = 0;

	assert(arg->stack == 0);

	/*
	 * If in -C or -L mode, flush the output collected so far.
	 * Otherwise, disregard it---it surrounds what we're interested
	 * in.
	 */

	if (arg->single != -1) {
		xmltextx(arg->f, arg->buf, arg->dst, 
			arg->sargs, arg->sposz, arg->sposz, 
			arg->single, arg->single, arg->sposz, 
			XMLESC_NONE);
		free(arg->buf);
		arg->buf = NULL;
		arg->bufsz = 0;
	}

	/*
	 * Whether to start an article or nav.  Articles start if we
	 * have a data-sblg-article attribute.  Navigation on any
	 * element with a data-sblg-nav attribute.
	 */

	for (attp = atts; *attp != NULL; attp += 2) {
		if (sblg_lookup(*attp) == SBLG_ATTR_ARTICLE &&
		    xmlbool(attp[1])) {
			start_article = 1;
			assert(arg->stacktag == NULL);
			arg->stacktag = xstrdup(s);
			break;
		}
		if (sblg_lookup(*attp) == SBLG_ATTR_NAV &&
		    xmlbool(attp[1])) {
			start_nav = 1;
			assert(arg->stacktag == NULL);
			arg->stacktag = xstrdup(s);
			break;
		}
	}

	if (start_nav) {
		arg->navsort = ASORT_DATE;
		arg->usesort = 0;
		arg->navlen = arg->sposz;
		arg->navstart = 0;
		arg->navelem = NAVELEM_KEEP;
		arg->navformat = NAVFORMAT_LIST_SUMMARISE;

		for (attp = atts; *attp != NULL; attp += 2)
			switch (sblg_lookup(*attp)) {
			case SBLG_ATTR_NAVCONTENT:
				/* DEPRECATED */
				if (xmlbool(attp[1])) {
					arg->navformat = NAVFORMAT_LIST_KEEP;
					arg->navelem = NAVELEM_KEEP;
				} else {
					arg->navformat = NAVFORMAT_LIST_SUMMARISE;
					arg->navelem = NAVELEM_KEEP;
				}
				break;
			case SBLG_ATTR_NAVSORT:
				sort = attp[1];
				break;
			case SBLG_ATTR_NAVSTART:
				arg->navstart = atoi(attp[1]);
				if (arg->navstart > (size_t)arg->sposz)
					arg->navstart = arg->sposz;
				if (arg->navstart)
					arg->navstart--;
				break;
			case SBLG_ATTR_NAVSTYLE_CONTENT:
				if (strcmp(attp[1], "keep") == 0)
					arg->navformat = NAVFORMAT_KEEP;
				else if (strcmp(attp[1], "summarise") == 0)
					arg->navformat = NAVFORMAT_SUMMARISE;
				else if (strcmp(attp[1], "summarize") == 0)
					arg->navformat = NAVFORMAT_SUMMARISE;
				else if (strcmp(attp[1], "list-keep") == 0)
					arg->navformat = NAVFORMAT_LIST_KEEP;
				else 
					arg->navformat = NAVFORMAT_LIST_SUMMARISE;
				break;
			case SBLG_ATTR_NAVSTYLE_ELEMENT:
				if (strcmp(attp[1], "keep-strip") == 0)
					arg->navelem = NAVELEM_KEEP_STRIP;
				else if (strcmp(attp[1], "repeat-strip") == 0)
					arg->navelem = NAVELEM_REPEAT_STRIP;
				else if (strcmp(attp[1], "repeat-strip") == 0)
					arg->navelem = NAVELEM_REPEAT_STRIP;
				else if (strcmp(attp[1], "discard") == 0)
					arg->navelem = NAVELEM_DISCARD;
				else
					arg->navelem = NAVELEM_KEEP;
				break;
			case SBLG_ATTR_NAVSZ:
				arg->navlen = atoi(attp[1]);
				if (arg->navlen > (size_t)arg->sposz)
					arg->navlen = arg->sposz;
				break;
			case SBLG_ATTR_NAVTAG:
				hashtag(&arg->navtags, 
					&arg->navtagsz, attp[1],
					arg->sargs, arg->sposz, arg->single);
				break;
			case SBLG_ATTR_NAVXML:
				/* DEPRECATED */
				if (xmlbool(attp[1])) {
					arg->navformat = NAVFORMAT_KEEP;
					arg->navelem = NAVELEM_DISCARD;
				} else {
					arg->navformat = NAVFORMAT_LIST_SUMMARISE;
					arg->navelem = NAVELEM_KEEP;
				}
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

		arg->stack++;
		arg->textmode = TEXT_NAV;
		XML_SetElementHandler(arg->p, nav_begin, nav_end);

		if (arg->navelem == NAVELEM_KEEP_STRIP)
			xmlopen(arg->f, s, NULL);
		else if (arg->navelem == NAVELEM_KEEP)
			xmlopens(arg->f, s, atts);
		if (arg->navformat == NAVFORMAT_LIST_SUMMARISE ||
		    arg->navformat == NAVFORMAT_LIST_KEEP)
			xmlopen(arg->f, "ul", NULL);
	} else if (start_article) {
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
			arg->textmode = TEXT_NONE;
			XML_SetElementHandler(arg->p, article_begin, article_end);
			return;
		}

		/* First throw away children, then push out the article. */

		arg->stack++;
		arg->textmode = TEXT_NONE;
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
	} else {
		if (arg->single != -1)
			xmlopensx(arg->f, s, atts, arg->dst,
				arg->sargs, arg->sposz, arg->single);
		else
			xmlopens(arg->f, s, atts);
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
linkall(XML_Parser p, const char *templ, const char *force, int sz,
    char *src[], const char *dst, enum asort asort)
{
	char		*buf = NULL;
	size_t		 j, ssz = 0;
	int		 i, fd = -1, rc = 0;
	FILE		*f = stdout;
	struct linkall	 arg;
	struct article	*sargs = NULL;
	size_t		 sargsz = 0;

	memset(&arg, 0, sizeof(struct linkall));

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

	arg.sargs = sargs;
	arg.sposz = arg.ssposz = sargsz;
	arg.p = p;
	arg.src = templ;
	arg.dst = strcmp(dst, "-") ? dst : NULL;
	arg.f = f;
	arg.single = -1;
	arg.textmode = TEXT_TMPL;
	arg.stacktag = NULL;

	if (force != NULL) {
		for (j = 0; j < sargsz; j++)
			if (strcmp(force, sargs[j].src) == 0)
				break;
		if (j < sargsz) {
			arg.single = j;
			arg.spos = j;
			arg.ssposz = j + 1;
		} else {
			warnx("%s: not in input list", force);
			goto out;
		}
	}

	/* Run the XML parser on the template. */

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, text);
	XML_SetSkippedEntityHandler(p, entity);
	XML_SetElementHandler(p, tmpl_begin, tmpl_end);
	XML_SetUserData(p, &arg);
	XML_UseForeignDTD(p, XML_TRUE);

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
	for (j = 0; j < arg.navtagsz; j++)
		free(arg.navtags[j]);
	free(arg.navtags);
	free(arg.nav);
	free(arg.buf);
	return rc;
}

/*
 * Like linkall() but does the output in place: groks all input files,
 * then converts them to output.
 * This prevents needing to run -C with each input file.
 * Return zero on fatal error, non-zero on success.
 */
int
linkall_r(XML_Parser p, const char *templ, int sz, char *src[],
    enum asort asort)
{
	char		*buf = NULL, *dst = NULL;
	size_t		 j, ssz = 0, wsz;
	int		 i, fd = -1, rc = 0;
	FILE		*f = NULL;
	struct linkall	 arg;
	struct article	*sargs = NULL;
	size_t		 sargsz = 0;
	const char	*cp;

	memset(&arg, 0, sizeof(struct linkall));

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

		arg.sargs = sargs;
		arg.sposz = sargsz;
		arg.p = p;
		arg.src = templ;
		arg.dst = dst;
		arg.f = f;
		arg.single = j;
		arg.spos = j;
		arg.ssposz = j + 1;
		arg.textmode = TEXT_TMPL;
		arg.stacktag = NULL;

		/* Run the XML parser on the template. */

		XML_ParserReset(p, NULL);
		XML_SetSkippedEntityHandler(p, entity);
		XML_SetDefaultHandlerExpand(p, text);
		XML_SetElementHandler(p, tmpl_begin, tmpl_end);
		XML_SetUserData(p, &arg);
		XML_UseForeignDTD(p, XML_TRUE);

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
	for (j = 0; j < arg.navtagsz; j++)
		free(arg.navtags[j]);
	free(arg.navtags);
	free(arg.nav);
	free(arg.buf);
	free(arg.stacktag);
	free(dst);
	return rc;
}
