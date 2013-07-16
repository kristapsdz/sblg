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
	int		 linked;
};

static	void	data_begin(void *userdata, 
			const XML_Char *name, const XML_Char **atts);
static	void	data_end(void *userdata, const XML_Char *name);
static	void	data_text(void *userdata, 
			const XML_Char *s, int len);
static	void	input_begin(void *userdata, const XML_Char *name, 
			const XML_Char **atts);

int
echo(FILE *f, int linked, const char *src)
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
	arg.linked = linked;

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

static void
input_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct input	 *arg = userdata;
	const XML_Char	**attp;

	if (strcasecmp(name, "article")) 
		return;

	/*
	 * Only handle articles with the magic attribute of
	 * data-sblg-article, else skip over them.
	 */
	for (attp = atts; NULL != *attp; attp++)
		if (0 == strcasecmp(*attp, "data-sblg-article"))
			break;

	if (1 == arg->linked && NULL == *attp)
		return;

	arg->stack++;
	XML_SetElementHandler(arg->p, data_begin, data_end);
	XML_SetDefaultHandler(arg->p, data_text);
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
