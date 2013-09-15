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
	FILE		*f; /* open template file */
	const char	*src; /* template file */
	XML_Parser	 p; /* active parser */
	struct article	*sargs; /* sorted article contents */
	int		 spos; /* current sarg being shown */ 
	int		 sposz; /* size of sargs */
	size_t		 stack; /* temporary: tag stack size */
	size_t		 navlen; /* temporary: nav items to show */
	int		 navuse; /* use navigation contents */
	char		*nav; /* temporary: nav buffer */
	size_t		 navsz; /* nav buffer length */
};

static	void	article_begin(void *userdata, 
			const XML_Char *name, const XML_Char **atts);
static	void	article_end(void *userdata, const XML_Char *name);
static	void	empty_end(void *userdata, const XML_Char *name);
static	void	nav_begin(void *userdata, 
			const XML_Char *name, const XML_Char **atts);
static	void	nav_end(void *userdata, const XML_Char *name);
static	void	nav_text(void *userdata, const XML_Char *s, int len);
static	int	scmp(const void *p1, const void *p2);
static	void	tmpl_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	tmpl_end(void *userdata, const XML_Char *name);
static	void	tmpl_text(void *userdata, 
			const XML_Char *s, int len);

int
linkall(XML_Parser p, const char *templ, 
		int sz, char *src[], const char *dst)
{
	char		*buf;
	size_t		 ssz;
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

	for (i = 0; i < sz; i++)
		if ( ! grok(p, 1, src[i], &sarg[i]))
			goto out;

	qsort(sarg, sz, sizeof(struct article), scmp);

	f = stdout;
	if (strcmp(dst, "-") && (NULL == (f = fopen(dst, "w")))) {
		perror(dst);
		goto out;
	} 
	
	if ( ! mmap_open(templ, &fd, &buf, &ssz))
		goto out;

	larg.sargs = sarg;
	larg.sposz = sz;
	larg.p = p;
	larg.src = templ;
	larg.f = f;

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandler(p, tmpl_text);
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
		grok_free(&sarg[i]);
	mmap_close(fd, buf, ssz);
	if (NULL != f && stdout != f)
		fclose(f);

	free(larg.nav);
	free(sarg);
	return(rc);
}

static void
tmpl_text(void *userdata, const XML_Char *s, int len)
{
	struct linkall	*arg = userdata;

	fprintf(arg->f, "%.*s", len, s);
}

static void
article_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct linkall	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "article");
}

static void
nav_text(void *userdata, const XML_Char *s, int len)
{
	struct linkall	*arg = userdata;

	xmlappend(&arg->nav, &arg->navsz, s, len);
}

static void
nav_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct linkall	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "nav");
	xmlrappendopen(&arg->nav, &arg->navsz, name, atts);
}

static void
nav_end(void *userdata, const XML_Char *name)
{
	struct linkall	*arg = userdata;
	size_t		 i, j, start;
	char		 buf[32];
	int		 inprint;

	if (strcasecmp(name, "nav") || 0 != --arg->stack) {
		xmlrappendclose(&arg->nav, &arg->navsz, name);
		return;
	}

	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	XML_SetDefaultHandler(arg->p, tmpl_text);

	fprintf(arg->f, "\n<ul>\n");

	if ( ! arg->navuse || 0 == arg->navsz) {
		for (i = 0; i < arg->navlen; i++) {
			strftime(buf, sizeof(buf), "%Y-%m-%d", 
				localtime(&arg->sargs[i].time));
			fprintf(arg->f, "<li>\n");
			fprintf(arg->f, "%s: ", buf);
			fprintf(arg->f, "<a href=\"%s\">%s</a>\n",
				arg->sargs[i].src,
				arg->sargs[i].title);
			fprintf(arg->f, "</li>\n");
		}
		fprintf(arg->f, "</ul>\n");
		fprintf(arg->f, "</%s>", name);
		free(arg->nav);
		arg->navsz = 0;
		arg->nav = NULL;
		return;
	}

#define	STRCMP(_word, _sz) (j - start == (_sz) && \
	0 == memcmp(&arg->nav[start], (_word), (_sz)))

	for (i = 0; i < arg->navlen; i++) {
		inprint = 0;
		fprintf(arg->f, "<li>\n");
		for (j = 1; j < arg->navsz; j++) {
			if ('$' != arg->nav[j - 1]) {
				fputc(arg->nav[j - 1], arg->f);
				continue;
			} else if ('{' != arg->nav[j]) {
				fputc(arg->nav[j - 1], arg->f);
				continue;
			}
			start = ++j;
			inprint = 1;
			for ( ; j < arg->navsz; j++) 
				if ('}' == arg->nav[j])
					break;
			if (j == arg->navsz)
				break;
			if (STRCMP("base", 4))
				fputs(arg->sargs[i].base, arg->f);
			else if (STRCMP("title", 5))
				fputs(arg->sargs[i].title, arg->f);
			else if (STRCMP("source", 6))
				fputs(arg->sargs[i].src, arg->f);

			if (j < arg->navsz)
				j++;
			inprint = 0;
		}
		if ( ! inprint)
			fputc(arg->nav[j - 1], arg->f);
		fprintf(arg->f, "</li>\n");
	}
	fprintf(arg->f, "</ul>\n");
	fprintf(arg->f, "</%s>", name);
	free(arg->nav);
	arg->navsz = 0;
	arg->nav = NULL;
}

static void
tmpl_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct linkall	 *arg = userdata;
	const XML_Char	**attp;

	assert(0 == arg->stack);

	if (0 == strcasecmp(name, "nav")) {
		/*
		 * Only handle if containing the "data-sblg-nav"
		 * attribute, otherwise continue.
		 */
		xmlprint(arg->f, name, atts);
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
		for (attp = atts; NULL != *attp; attp += 2) {
			if (0 == strcasecmp(attp[0], 
					"data-sblg-navsz")) {
				arg->navlen = atoi(attp[1]);
				if (arg->navlen > (size_t)arg->sposz)
					arg->navlen = arg->sposz;
			} else if (0 == strcasecmp(attp[0], 
					"data-sblg-navcontent"))
				arg->navuse = xmlbool(attp[1]);
		}

		arg->stack++;
		XML_SetElementHandler(arg->p, nav_begin, nav_end);
		XML_SetDefaultHandler(arg->p, nav_text);
		return;
	} else if (strcasecmp(name, "article")) {
		xmlprint(arg->f, name, atts);
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
		xmlprint(arg->f, name, atts);
		return;
	}

	if (arg->sposz <= arg->spos) {
		/*
		 * We have no articles left to show.
		 * Just continue throwing away this article element til
		 * we receive a matching one.
		 */
		arg->stack++;
		XML_SetDefaultHandler(arg->p, NULL);
		XML_SetElementHandler(arg->p, article_begin, empty_end);
		return;
	}

	/*
	 * First throw away children, then push out the article itself.
	 */
	xmlprint(arg->f, name, atts);
	arg->stack++;
	XML_SetDefaultHandler(arg->p, NULL);
	XML_SetElementHandler(arg->p, article_begin, article_end);
	if ( ! echo(arg->f, 1, arg->sargs[arg->spos++].src))
		XML_StopParser(arg->p, 0);
	fprintf(arg->f, "</%s>\n", name);
	for (attp = atts; NULL != *attp; attp += 2) 
		if (0 == strcasecmp(*attp, "data-sblg-permlink"))
			break;
	if (NULL != *attp && ! xmlbool(attp[1]))
		return;
	fprintf(arg->f, "<div data-sblg-permlink=\"1\"><a href=\"%s\">"
			"permanent link</a></div>", 
			arg->sargs[arg->spos - 1].src);
}

static void
empty_end(void *userdata, const XML_Char *name)
{
	struct linkall	*arg = userdata;

	if (0 == strcasecmp(name, "article") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandler(arg->p, tmpl_text);
	}
}

static void
tmpl_end(void *userdata, const XML_Char *name)
{
	struct linkall	*arg = userdata;

	if ( ! xmlvoid(name))
		fprintf(arg->f, "</%s>", name);
}

static void
article_end(void *userdata, const XML_Char *name)
{
	struct linkall	*arg = userdata;

	if (0 == strcasecmp(name, "article") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandler(arg->p, tmpl_text);
	}
}

static int
scmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	return(difftime(s2->time, s1->time));
}
