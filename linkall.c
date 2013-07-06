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

struct	input {
	FILE		*f;
	XML_Parser	 p;
	size_t		 stack;
	int 		 inparse;
};

struct	linkall {
	FILE		*f;
	const char	*src;
	XML_Parser	 p;
	struct article	*sargs;
	int		 spos;
	int		 sposz;
	size_t		 stack;
};

static	void	article_begin(void *userdata, 
			const XML_Char *name, const XML_Char **atts);
static	void	article_end(void *userdata, const XML_Char *name);
static	void	empty_end(void *userdata, const XML_Char *name);
static	void	input_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	data_begin(void *userdata, 
			const XML_Char *name, const XML_Char **atts);
static	void	data_end(void *userdata, const XML_Char *name);
static	void	data_text(void *userdata, 
			const XML_Char *s, int len);
static	void	nav_begin(void *userdata, 
			const XML_Char *name, const XML_Char **atts);
static	void	nav_end(void *userdata, const XML_Char *name);
static	int	scmp(const void *p1, const void *p2);
static	void	tmpl_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	tmpl_end(void *userdata, const XML_Char *name);
static	void	tmpl_text(void *userdata, 
			const XML_Char *s, int len);

static int
input(FILE *f, const char *src)
{
	char		*buf;
	XML_Parser	 p;
	size_t		 sz;
	int		 fd, rc;
	struct input	 arg;

	rc = 0;
	memset(&arg, 0, sizeof(struct input));

	if (NULL == (p = XML_ParserCreate(NULL))) {
		perror(NULL);
		exit(EXIT_FAILURE);
	} else if ( ! mmap_open(src, &fd, &buf, &sz))
		goto out;

	arg.f = f;
	arg.p = p;

	XML_SetElementHandler(p, input_begin, NULL);
	XML_SetUserData(p, &arg);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)sz, 1)) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", src, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	rc = 1;
out:
	mmap_close(fd, buf, sz);
	if (NULL != p)
		XML_ParserFree(p);
	return(rc);
}

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
		if ( ! grok(p, src[i], &sarg[i]))
			goto out;

	qsort(sarg, sz, sizeof(struct article), scmp);

	if (NULL == (f = fopen(dst, "w"))) {
		perror(dst);
		goto out;
	} else if ( ! mmap_open(templ, &fd, &buf, &ssz))
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
	if (NULL != f)
		fclose(f);

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
data_text(void *userdata, const XML_Char *s, int len)
{
	struct input	*arg = userdata;

	fprintf(arg->f, "%.*s", len, s);
}

static void
data_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct input	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "article");
	xmlprint(arg->f, name, atts);
}

static void
input_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct input	*arg = userdata;

	if (0 == strcasecmp(name, "article")) {
		arg->stack++;
		XML_SetElementHandler(arg->p, data_begin, data_end);
		XML_SetDefaultHandler(arg->p, data_text);
	}
}

static void
data_end(void *userdata, const XML_Char *name)
{
	struct input	*arg = userdata;
	
	if (strcasecmp(name, "article")) {
		if ( ! xmlvoid(name))
			fprintf(arg->f, "</%s>", name);
		return;
	} if (--arg->stack > 0) {
		fprintf(arg->f, "</%s>", name);
		return;
	}

	XML_SetDefaultHandler(arg->p, NULL);
	XML_SetElementHandler(arg->p, NULL, NULL);
}

static void
article_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct linkall	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "article");
}

static void
nav_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct linkall	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "nav");
}

static void
nav_end(void *userdata, const XML_Char *name)
{
	struct linkall	*arg = userdata;

	if (0 == strcasecmp(name, "nav") && 0 == --arg->stack) {
		fprintf(arg->f, "</%s>", name);
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandler(arg->p, tmpl_text);
	}
}

static void
tmpl_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct linkall	*arg = userdata;
	size_t		 i;
	char		 buf[1024];

	assert(0 == arg->stack);

	if (0 == strcasecmp(name, "nav")) {
		xmlprint(arg->f, name, atts);
		fprintf(arg->f, "<ul>\n");
		for (i = 0; i < (size_t)arg->sposz; i++) {
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
		arg->stack++;
		XML_SetDefaultHandler(arg->p, NULL);
		XML_SetElementHandler(arg->p, nav_begin, nav_end);
		return;
	} else if (strcasecmp(name, "article")) {
		xmlprint(arg->f, name, atts);
		return;
	}

	if (arg->sposz <= arg->spos) {
		arg->stack++;
		XML_SetDefaultHandler(arg->p, NULL);
		XML_SetElementHandler(arg->p, article_begin, empty_end);
		return;
	}

	xmlprint(arg->f, name, atts);
	arg->stack++;
	XML_SetDefaultHandler(arg->p, NULL);
	XML_SetElementHandler(arg->p, article_begin, article_end);
	if ( ! input(arg->f, arg->sargs[arg->spos++].src))
		XML_StopParser(arg->p, 0);
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
		fprintf(arg->f, "</%s>", name);
		fprintf(arg->f, "<div><a href=\"%s\">"
				"permanent link</a></div>\n", 
				arg->sargs[arg->spos - 1].src);
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
