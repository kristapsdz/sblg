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
	TEXT_TMPL,
	TEXT_NONE,
};

struct	pargs {
	const char	*src; /* template file */
	const char	*dst; /* output file (or empty) */
	FILE		*f; /* output stream */
	XML_Parser	 p; /* active parser */
	size_t		 stack; /* temporary: tag stack size */
	struct article	*article; /* standalone article */
	char		*buf; /* buffer for text */
	size_t		 bufsz; /* buffer size */
	size_t		 bufmax; /* buffer maximum size */
	enum textmode	 textmode; /* mode to accept text */
	char		*stacktag; /* tag starting article or NULL */
};

static void
text(void *dat, const XML_Char *s, int len)
{
	struct pargs	*arg = dat;

	switch (arg->textmode) {
	case TEXT_TMPL:
		xmlstrtext(&arg->buf, &arg->bufsz, s, len);
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
template_end(void *dat, const XML_Char *s)
{
	struct pargs	*arg = dat;

	xmltextx(arg->f, arg->buf, arg->dst, 
		arg->article, 1, 1, 0, 0, 1, XMLESC_NONE);
	free(arg->buf);
	arg->buf = NULL;
	arg->bufsz = 0;
	xmlclose(arg->f, s);
}

/*
 * Record the stack of nested <article> entries so we don't prematurely
 * close the article context.
 */
static void
article_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct pargs	*arg = dat;

	assert(arg->stacktag != NULL);
	arg->stack += strcmp(s, arg->stacktag) == 0;
}

/*
 * If we're at the bottom of the stack of <article> entries, we're
 * finished with this article; print the rest verbatim.
 */
static void
article_end(void *dat, const XML_Char *s)
{
	struct pargs	*arg = dat;

	assert(arg->stacktag != NULL);
	if (strcmp(s, arg->stacktag) != 0 || --arg->stack != 0)
		return;

	free(arg->stacktag);
	arg->stacktag = NULL;
	arg->textmode = TEXT_TMPL;
	XML_SetElementHandler(arg->p, NULL, NULL);
}

/*
 * Look for important tags in the template.
 * This is the main handler for this file.
 */
static void
template_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct pargs	 *arg = dat;
	const XML_Char	**attp;
	int		  start_article = 0;

	assert(arg->stack == 0);

	xmltextx(arg->f, arg->buf, arg->dst, 
		arg->article, 1, 1, 0, 0, 1, XMLESC_NONE);

	free(arg->buf);
	arg->buf = NULL;
	arg->bufsz = 0;

	/* Look for the true-valued data-sblg-article.  */

	for (attp = atts; *attp != NULL; attp += 2)
		if (sblg_lookup(attp[0]) == SBLG_ATTR_ARTICLE &&
		    xmlbool(attp[1])) {
			start_article = 1;
			break;
		}

	if (!start_article) {
		xmlopensx(arg->f, s, atts, 
			arg->dst, arg->article, 1, 0);
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

	assert(arg->stacktag == NULL);
	arg->stacktag = xstrdup(s);

	/*
	 * If we encounter an <article data-sblg-article>, then echo
	 * the article file and discard content until the matching close
	 * of the article.
	 */

	arg->stack++;
	arg->textmode = TEXT_NONE;
	XML_SetElementHandler(arg->p, article_begin, article_end);
	xmltextx(arg->f, arg->article->article, arg->dst,
		arg->article, 1, 1, 0, 0, 1, XMLESC_NONE);
}

/*
 * Merge a single input XML file into a template XML files to produce
 * output.
 * (This can happen multiple times if we're spitting into stdout.)
 * Return zero on fatal error, non-zero on success.
 */
int
compile(XML_Parser p, const char *templ, 
	const char *src, const char *dst)
{
	char		*out = NULL, *cp, *buf = NULL;
	size_t		 sz = 0, sargsz = 0;
	int		 fd = -1, rc = 0;
	FILE		*f = stdout;
	struct pargs	 arg;
	struct article	*sargs = NULL;

	memset(&arg, 0, sizeof(struct pargs));

	if (!sblg_parse(p, src, &sargs, &sargsz, NULL))
		goto out;

	if (sargsz == 0) {
		warnx("%s: contains no article", src);
		goto out;
	} else if (sargsz > 1)
		warnx("%s: contains multiple "
			"articles (using the first)", src);

	arg.article = &sargs[0];

	/*
	 * If we have no output file name, then name it the same as the
	 * input but with ".html" at the end.
	 * However, if we have ".xml", then replace that with ".html".
	 */

	if (dst == NULL) {
		sz = strlen(src);
		if ((cp = strrchr(src, '.')) == NULL ||
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

	if (strcmp(out, "-") && (f = fopen(out, "w")) == NULL) {
		warn("%s", out);
		goto out;
	} 

	if (!mmap_open(templ, &fd, &buf, &sz))
		goto out;

	arg.f = f;
	arg.src = src;
	arg.dst = strcmp(out, "-") ? out : NULL;
	arg.p = p;
	arg.textmode = TEXT_TMPL;
	arg.stacktag = NULL;

	XML_ParserReset(p, NULL);
	XML_SetElementHandler(p, template_begin, template_end);
	XML_SetSkippedEntityHandler(p, entity);
	XML_SetDefaultHandlerExpand(p, text);
	XML_SetUserData(p, &arg);
	XML_UseForeignDTD(p, XML_TRUE);

	if (XML_Parse(p, buf, (int)sz, 1) != XML_STATUS_OK) {
		warnx("%s:%zu:%zu: %s", templ, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	xmltextx(arg.f, arg.buf, arg.dst, 
		arg.article, 1, 1, 0, 0, 1, XMLESC_NONE);
	fputc('\n', f);
	rc = 1;
out:
	mmap_close(fd, buf, sz);
	if (f != NULL && f != stdin)
		fclose(f);
	sblg_free(sargs, sargsz);
	free(out);
	free(arg.buf);
	free(arg.stacktag);
	return rc;
}

