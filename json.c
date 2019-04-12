/*	$Id$ */
/*
 * Copyright (c) 2016--2017 Kristaps Dzonsons <kristaps@bsd.lv>,
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

#include <sys/param.h>

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
#include "version.h"

/*
 * FIXME: use strcspn().
 */
static void
json_quoted(const char *cp, FILE *f)
{
	char	 c;

	fputc('"', f);
	while ((c = *cp++) != '\0')
		switch (c) {
		case ('"'):
		case ('\\'):
		case ('/'):
			fputc('\\', f);
			fputc(c, f);
			break;
		case ('\b'):
			fputs("\\b", f);
			break;
		case ('\f'):
			fputs("\\f", f);
			break;
		case ('\n'):
			fputs("\\n", f);
			break;
		case ('\r'):
			fputs("\\r", f);
			break;
		case ('\t'):
			fputs("\\t", f);
			break;
		default:
			fputc(c, f);
			break;
		}
	fputc('"', f);
}

static void
json_time(const char *key, time_t t, FILE *f)
{

	json_quoted(key, f);
	fputc(':', f);
	fprintf(f, "%lld", (long long)t);
}

static void
json_text(const char *key, const char *text, FILE *f)
{

	json_quoted(key, f);
	fputc(':', f);
	json_quoted(text, f);
}

static void
json_textxml(const char *key, 
	const char *text, const char *xml, FILE *f)
{

	json_quoted(key, f);
	fputc(':', f);
	fputc('{', f);
	if (text != NULL) {
		json_quoted("text", f);
		fputc(':', f);
		json_quoted(text, f);
		fputc(',', f);
	}
	json_quoted("xml", f);
	fputc(':', f);
	json_quoted(xml, f);
	fputc('}', f);
}

static void
json_textlist(const char *key, char **tags, size_t tagsz, FILE *f)
{
	size_t	 i;
	
	json_quoted("tags", f);
	fputc(':', f);
	fputc('[', f);

	for (i = 0; i < tagsz; i++) {
		if (i > 0)
			fputc(',', f);
		json_quoted(tags[i], f);
	}

	fputc(']', f);
}

/*
 * Format entire articles into JSON output.
 * I still don't have the schema really documented except in the
 * manpage, so this should probably receive more attention to make it
 * more in-line with JSON expectations.
 */
int
json(XML_Parser p, int sz, char *src[], const char *dst, enum asort asort)
{
	size_t		 j, sargsz = 0;
	int		 i, rc = 0;
	FILE		*f = stdout;
	struct article	*sargs = NULL;

	for (i = 0; i < sz; i++)
		if (!sblg_parse(p, src[i], &sargs, &sargsz, NULL))
			goto out;

	sblg_sort(sargs, sargsz, asort);

	if (strcmp(dst, "-") && (f = fopen(dst, "w")) == NULL) {
		warn("%s", dst);
		goto out;
	}

	fputc('{', f);
	json_text("version", VERSION, f);
	fputc(',', f);
	json_quoted("articles", f);
	fputs(": [", f);

	for (j = 0; j < sargsz; j++) {
		fputc('{', f);
		json_text("src", sargs[j].src, f);
		fputc(',', f);
		json_text("base", sargs[j].base, f);
		fputc(',', f);
		json_text("stripbase", 
			sargs[j].stripbase, f);
		fputc(',', f);
		json_text("striplangbase", 
			sargs[j].striplangbase, f);
		fputc(',', f);
		json_time("time", sargs[j].time, f);
		fputc(',', f);
		json_textxml("title", 
			sargs[j].titletext, 
			sargs[j].title, f);
		fputc(',', f);
		json_textxml("aside", 
			sargs[j].asidetext, 
			sargs[j].aside, f);
		fputc(',', f);
		json_textxml("author", 
			sargs[j].authortext, 
			sargs[j].author, f);
		fputc(',', f);
		json_textxml("article", NULL,
			sargs[j].article, f);
		fputc(',', f);
		json_textlist("tags", sargs[j].tagmap, 
			sargs[j].tagmapsz, f);
		fputc('}', f);
		if (j < sargsz - 1)
			fputc(',', f);
	}
	fputs("]}\n", f);

	rc = 1;
out:
	sblg_free(sargs, sargsz);
	if (f != NULL && f != stdout)
		fclose(f);
	return rc;
}

