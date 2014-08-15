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
	const char	*src; /* template file */
	const char	*dst; /* output file (or empty) */
	FILE		*f; /* output stream */
	XML_Parser	 p; /* active parser */
	size_t		 stack; /* temporary: tag stack size */
	struct article	 article; /* standalone article */
	char		*buf; /* buffer for text */
	size_t		 bufsz; /* buffer size */
	size_t		 bufmax; /* buffer maximum size */
};

static void
template_text(void *dat, const XML_Char *s, int len)
{
	struct pargs	*arg = dat;

	xmlstrtext(&arg->buf, &arg->bufsz, s, len);
}

static void
template_end(void *dat, const XML_Char *name)
{
	struct pargs	*arg = dat;

	xmltextx(arg->f, arg->buf, arg->dst, &arg->article);
	xmlstrflush(arg->buf, &arg->bufsz);
	xmlclose(arg->f, name);
}

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
		XML_SetDefaultHandlerExpand(arg->p, template_text);
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

	xmltextx(arg->f, arg->buf, arg->dst, &arg->article);
	xmlstrflush(arg->buf, &arg->bufsz);

	if (strcasecmp(name, "article")) {
		xmlopensx(arg->f, name, atts, arg->dst, &arg->article);
		return;
	}

	for (attp = atts; NULL != *attp; attp += 2)
		if (0 == strcasecmp(*attp, "data-sblg-article"))
			break;

	if (NULL == *attp || ! xmlbool(attp[1])) {
		xmlopensx(arg->f, name, atts, arg->dst, &arg->article);
		return;
	}

	/*
	 * If we encounter an <article data-sblg-article="1">, then echo
	 * the article file and discard content until the matching close
	 * of the article.
	 */
	arg->stack++;
	XML_SetElementHandler(arg->p, article_begin, article_end);
	XML_SetDefaultHandlerExpand(arg->p, NULL);
	if ( ! echo(arg->f, 0, arg->src))
		XML_StopParser(arg->p, 0);
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
			strlcpy(out, src, sz - 2);
			strlcat(out, "html", sz + 2);
		} 
	} else
		out = xstrdup(dst);

	f = stdout;
	if (strcmp(out, "-") && NULL == (f = fopen(out, "w"))) {
		perror(out);
		goto out;
	} 
	if ( ! mmap_open(templ, &fd, &buf, &sz))
		goto out;

	arg.f = f;
	arg.src = src;
	arg.dst = strcmp(out, "-") ? out : NULL;
	arg.p = p;

	XML_ParserReset(p, NULL);
	XML_SetElementHandler(p, template_begin, template_end);
	XML_SetDefaultHandlerExpand(p, template_text);
	XML_SetUserData(p, &arg);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)sz, 1)) {
		fprintf(stderr, "%s:%zu:%zu: %s\n", templ, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	xmltextx(arg.f, arg.buf, arg.dst, &arg.article);
	xmlstrflush(arg.buf, &arg.bufsz);
	fputc('\n', f);
	rc = 1;
out:
	mmap_close(fd, buf, sz);
	if (NULL != f && stdin != f)
		fclose(f);

	free(out);
	article_free(&arg.article);
	free(arg.buf);
	return(rc);
}

