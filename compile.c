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
#include <assert.h>
#include <expat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

struct	pargs {
	const char	*src;
	FILE		*f;
	XML_Parser	 p;
	size_t		 stack;
	struct article	 article;
};

/* 
 * Forward-declarations for circular dependencies. 
 */
static void	template_begin(void *dat, const XML_Char *name, 
			const XML_Char **atts);
static void	template_end(void *dat, const XML_Char *name);
static void	template_text(void *dat, const XML_Char *s, int len);

/*
 * Record the stack of nested <article> entries so we don't prematurely
 * close the article context.
 */
static void
article_begin(void *dat, const XML_Char *name, const XML_Char **atts)
{
	struct pargs	*arg = dat;

	arg->stack += 0 == strcasecmp(name, "article");
}

/*
 * If we're at the bottom of the stack of <article> entries, we're
 * finished with this article; print the rest verbatim.
 */
static void
article_end(void *dat, const XML_Char *name)
{
	struct pargs	*arg = dat;

	if (0 == strcasecmp(name, "article") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, NULL, NULL);
		XML_SetDefaultHandler(arg->p, template_text);
	}
}

/*
 * Start a stack of nested <title> elements.
 */
static void
title_begin(void *dat, const XML_Char *name, const XML_Char **atts)
{
	struct pargs	*arg = dat;

	arg->stack += 0 == strcasecmp(name, "title");
}

/*
 * When we've finished our <title> element, go back into the main
 * parsing context looking for <article> elements.
 */
static void
title_end(void *dat, const XML_Char *name)
{
	struct pargs	*arg = dat;

	if (0 == strcasecmp(name, "title") && 0 == --arg->stack) {
		fprintf(arg->f, "</%s>", name);
		XML_SetElementHandler(arg->p, template_begin, template_end);
		XML_SetDefaultHandler(arg->p, template_text);
	}
}

/*
 * Look for important tags in the template.
 * This is the main handler for this file.
 */
static void
template_begin(void *dat, const XML_Char *name, const XML_Char **atts)
{
	struct pargs	 *arg = dat;
	const XML_Char	**attp;

	assert(0 == arg->stack);

	/*
	 * If we encounter [any!] title, prepend the article's title in front of it.
	 * FIXME: note this with data-sblg-title="1" or something like
	 * that, and also allow for appending instead of prepending; or
	 * better yet, a template.
	 */
	if (0 == strcasecmp(name, "title")) {
		xmlprint(arg->f, name, atts);
		fprintf(arg->f, "%s", arg->article.title);
		arg->stack++;
		XML_SetElementHandler(arg->p, title_begin, title_end);
		XML_SetDefaultHandler(arg->p, NULL);
		return;
	} else if (strcasecmp(name, "article")) {
		xmlprint(arg->f, name, atts);
		return;
	}

	for (attp = atts; NULL != *attp; attp += 2)
		if (0 == strcasecmp(*attp, "data-sblg-article"))
			break;

	if (NULL == *attp || ! xmlbool(attp[1])) {
		xmlprint(arg->f, name, atts);
		return;
	}

	/*
	 * If we encounter an <article data-sblg-article="1">, then echo
	 * the article file and discard content until the matching close
	 * of the article.
	 */
	arg->stack++;
	XML_SetElementHandler(arg->p, article_begin, article_end);
	XML_SetDefaultHandler(arg->p, NULL);
	if ( ! echo(arg->f, 0, arg->src))
		XML_StopParser(arg->p, 0);
}

/*
 * Blindly echo content.
 */
static void
template_text(void *dat, const XML_Char *s, int len)
{
	struct pargs	*arg = dat;

	fprintf(arg->f, "%.*s", len, s);
}

/*
 * Blindly echo end tags (unless void elements!).
 */
static void
template_end(void *dat, const XML_Char *name)
{
	struct pargs	*arg = dat;

	/* FIXME: put this all into xmlclose() or similar. */
	if ( ! xmlvoid(name))
		fprintf(arg->f, "</%s>", name);
}

int
compile(XML_Parser p, const char *templ, 
		const char *src, const char *dst)
{
	char		*out, *cp, *buf;
	size_t		 sz;
	int		 fd, rc;
	FILE		*f;
	struct pargs	 arg;

	memset(&arg, 0, sizeof(struct pargs));

	rc = 0;
	buf = out = NULL;
	fd = -1;
	f = NULL;
	sz = 0;

	if ( ! grok(p, src, &arg.article))
		goto out;

	if (NULL == dst) {
		/*
		 * If we have no output file name, then name it the same
		 * as the input but with ".html" at the end.
		 * However, if we have ".xml", then replace that with
		 * ".html".
		 */
		sz = strlen(src);
		if (NULL == (cp = strrchr(src, '.')) ||
				strcasecmp(cp + 1, "xml")) {
			/* Append .html to input name. */
			out = xmalloc(sz + 6);
			strlcpy(out, src, sz + 6);
			strlcat(out, ".html", sz + 6);
		} else {
			/* Replace .xml with .html. */
			out = xmalloc(sz + 2);
			strlcpy(out, src, sz - 3);
			strlcat(out, "html", sz + 2);
		} 
	} else
		out = xstrdup(dst);

	f = stdout;
	if (strcmp(out, "-") && NULL == (f = fopen(out, "w"))) {
		perror(out);
		goto out;
	} else if ( ! mmap_open(templ, &fd, &buf, &sz))
		goto out;

	arg.f = f;
	arg.src = src;
	arg.p = p;

	XML_ParserReset(p, NULL);
	XML_SetElementHandler(p, template_begin, template_end);
	XML_SetDefaultHandler(p, template_text);
	XML_SetUserData(p, &arg);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)sz, 1)) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", templ, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	fputc('\n', f);
	rc = 1;
out:
	mmap_close(fd, buf, sz);
	if (NULL != f && stdin != f)
		fclose(f);

	free(out);
	grok_free(&arg.article);
	return(rc);
}

