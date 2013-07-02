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
#include <err.h>
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
};

static	void	elem_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	elem_bbegin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	elem_end(void *userdata, const XML_Char *name);
static	void	elem_eend(void *userdata, const XML_Char *name);
static	void	elem_text(void *userdata, const XML_Char *s, int len);

static int
output(FILE *f, const char *src)
{
	char		 buf[BUFSIZ];
	ssize_t		 ssz;
	int		 fd;

	if (-1 == (fd = open(src, O_RDONLY, 0))) {
		warn("%s", src);
		return(0);
	}

	while ((ssz = read(fd, buf, BUFSIZ)) > 0)
		fwrite(buf, (size_t)ssz, 1, f);

	if (ssz < 0) {
		warn("%s", src);
		close(fd);
		return(0);
	}

	close(fd);
	return(1);
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
	struct article	 article;

	memset(&arg, 0, sizeof(struct pargs));

	rc = 0;
	buf = out = NULL;
	fd = -1;
	f = NULL;
	sz = 0;

	if ( ! grok(p, src, &article))
		goto out;

	if (NULL == dst) {
		if (NULL == (cp = strrchr(src, '.'))) {
			warnx("bad filename: %s", src);
			goto out;
		} else if (strcasecmp(++cp, "xml")) {
			warnx("bad suffix: %s", src);
			goto out;
		}
		sz = strlen(src);
		out = xmalloc(sz + 2);
		strlcpy(out, src, sz + 2);
		cp = strrchr(out, '.');
		cp[1] = '\0';
		strlcat(out, "html", sz + 2);
		sz = 0;
	} else
		out = xstrdup(dst);

	if (NULL == (f = fopen(out, "w"))) {
		warn("%s", out);
		goto out;
	} else if ( ! mmap_open(templ, &fd, &buf, &sz))
		goto out;

	arg.f = f;
	arg.src = src;
	arg.p = p;

	XML_ParserReset(p, NULL);
	XML_SetElementHandler(p, elem_begin, elem_eend);
	XML_SetDefaultHandler(p, elem_text);
	XML_SetUserData(p, &arg);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)sz, 1)) {
		warnx("%s: %s", templ, 
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	fputc('\n', f);
	rc = 1;
out:
	mmap_close(fd, buf, sz);
	if (NULL != f)
		fclose(f);

	free(out);
	free(article.title);
	return(rc);
}

static void
elem_bbegin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct pargs	*arg = userdata;

	if (0 == strcasecmp(name, "article"))
		arg->stack++;
}

static void
elem_begin(void *userdata, const XML_Char *name, const XML_Char **atts)
{
	struct pargs	*arg = userdata;

	xmlprint(arg->f, name, atts);

	if (strcasecmp(name, "article"))
		return;

	fputc('\n', arg->f);

	XML_SetElementHandler(arg->p, elem_bbegin, elem_end);
	XML_SetDefaultHandler(arg->p, NULL);

	fprintf(arg->f, "<!-- sblg writing fragment... -->\n");
	if ( ! output(arg->f, arg->src))
		XML_StopParser(arg->p, 0);
	fprintf(arg->f, "<!-- ...sblg finished fragment -->\n");
}

static void
elem_eend(void *userdata, const XML_Char *name)
{
	struct pargs	*arg = userdata;

	if ( ! xmlvoid(name))
		fprintf(arg->f, "</%s>", name);
}

static void
elem_end(void *userdata, const XML_Char *name)
{
	struct pargs	*arg = userdata;

	if (strcasecmp(name, "article"))
		return;
	else if (arg->stack-- > 0)
		return;

	fprintf(arg->f, "</%s>", name);
	XML_SetElementHandler(arg->p, NULL, NULL);
	XML_SetDefaultHandler(arg->p, elem_text);
}

static void
elem_text(void *userdata, const XML_Char *s, int len)
{
	struct pargs	*arg = userdata;

	fprintf(arg->f, "%.*s", len, s);
}
