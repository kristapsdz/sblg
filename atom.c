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

struct	atom {
	FILE		*f;
	const char	*src;
	XML_Parser	 p;
	struct article	*sargs;
	int		 spos;
	int		 sposz;
	size_t		 stack;
};

static	void	entry_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	entry_empty(void *userdata, const XML_Char *name);
static	void	entry_end(void *userdata, const XML_Char *name);
static	int	scmp(const void *p1, const void *p2);
static	void	template_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);
static	void	template_end(void *userdata, const XML_Char *name);
static	void	template_text(void *userdata, 
			const XML_Char *s, int len);

static int
atomprint(FILE *f, const struct article *src)
{
	char		 buf[1024];
	struct tm	*tm;

	tm = localtime(&src->time);
	strftime(buf, sizeof(buf), "%FT%TZ", tm);

	fprintf(f, "<title>%s</title>\n", src->title);
	fprintf(f, "<id>/%s</id>\n", src->src);
	fprintf(f, "<updated>%s</updated>\n", buf);
	fprintf(f, "<author><name>%s</name></author>\n", src->author);
	fprintf(f, "<link rel=\"alternate\" "
			 "type=\"text/html\" "
			 "href=\"/%s\" />\n", src->src);
	return(1);
}

int
atom(XML_Parser p, const char *templ, 
		int sz, char *src[], const char *dst)
{
	char		*buf;
	size_t		 ssz;
	int		 i, fd, rc;
	FILE		*f;
	struct atom	 larg;
	struct article	*sarg;

	ssz = 0;
	rc = 0;
	buf = NULL;
	fd = -1;
	f = NULL;

	memset(&larg, 0, sizeof(struct atom));
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
	XML_SetDefaultHandler(p, template_text);
	XML_SetElementHandler(p, template_begin, template_end);
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
template_text(void *userdata, const XML_Char *s, int len)
{
	struct atom	*arg = userdata;

	fprintf(arg->f, "%.*s", len, s);
}

static void
entry_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct atom	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "entry");
}

static void
template_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct atom	*arg = userdata;
	time_t		 t;
	char		 buf[1024];
	struct tm	*tm;

	if (0 == strcasecmp(name, "updated")) {
		t = arg->sposz <= arg->spos ?
			time(NULL) :
			arg->sargs[arg->spos].time;
		tm = localtime(&t);
		strftime(buf, sizeof(buf), "%FT%TZ", tm);
		xmlprint(arg->f, name, atts);
		fprintf(arg->f, "%s", buf);
		return;
	} else if (strcasecmp(name, "entry")) {
		xmlprint(arg->f, name, atts);
		return;
	}

	assert(0 == arg->stack);
	arg->stack++;

	if (arg->sposz <= arg->spos) {
		XML_SetDefaultHandler(arg->p, NULL);
		XML_SetElementHandler(arg->p, entry_begin, entry_empty);
		return;
	}

	xmlprint(arg->f, name, atts);
	XML_SetDefaultHandler(arg->p, NULL);
	XML_SetElementHandler(arg->p, entry_begin, entry_end);
	if ( ! atomprint(arg->f, &arg->sargs[arg->spos++]))
		XML_StopParser(arg->p, 0);
}

static void
entry_empty(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;

	if (0 == strcasecmp(name, "entry") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, template_begin, template_end);
		XML_SetDefaultHandler(arg->p, template_text);
	}
}

static void
template_end(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;

	if ( ! xmlvoid(name))
		fprintf(arg->f, "</%s>", name);
}

static void
entry_end(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;

	if (strcasecmp(name, "entry") || --arg->stack > 0)
		return;

	fprintf(arg->f, "</%s>", name);
	XML_SetElementHandler(arg->p, template_begin, template_end);
	XML_SetDefaultHandler(arg->p, template_text);
}

static int
scmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	return(difftime(s2->time, s1->time));
}
