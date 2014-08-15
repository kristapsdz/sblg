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
#include <sys/mman.h>
#include <sys/stat.h>

#include <expat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

/*
 * All possible self-closing (e.g., <area />) elements.
 */
static	const char *elemvoid[] = {
	"area",
	"base",
	"br",
	"col",
	"command",
	"embed",
	"hr",
	"img",
	"input",
	"keygen",
	"link",
	"meta",
	"param",
	"source",
	"track",
	"wbr",
	NULL
};

/*
 * Look up a given element to see if it can be "void", i.e., close
 * itself out.
 * For example, <p> is not void; <link /> is.
 */
static int
xmlvoid(const XML_Char *s)
{
	const char	**cp;

	for (cp = (const char **)elemvoid; NULL != *cp; cp++)
		if (0 == strcasecmp(s, *cp))
			return(1);

	return(0);
}

/*
 * Map a regular file into memory for parsing.
 * Make sure it's not too large, first.
 */
int
mmap_open(const char *f, int *fd, char **buf, size_t *sz)
{
	struct stat	 st;

	*fd = -1;
	*buf = NULL;
	*sz = 0;

	if (-1 == (*fd = open(f, O_RDONLY, 0))) {
		perror(f);
		goto out;
	} else if (-1 == fstat(*fd, &st)) {
		perror(f);
		goto out;
	} else if ( ! S_ISREG(st.st_mode)) {
		fprintf(stderr, "%s: not a regular file\n", f);
		goto out;
	} else if (st.st_size >= (1U << 31)) {
		fprintf(stderr, "%s: too large", f);
		goto out;
	}

	*sz = (size_t)st.st_size;
	*buf = mmap(NULL, *sz, PROT_READ, MAP_FILE|MAP_SHARED, *fd, 0);

	if (MAP_FAILED == *buf) {
		perror(f);
		goto out;
	}

	return(1);
out:
	mmap_close(*fd, *buf, *sz);
	return(0);
}

/*
 * Reverse of mmap_open.
 * Do NOT call twice!
 */
void
mmap_close(int fd, void *buf, size_t sz)
{

	if (NULL != buf)
		munmap(buf, sz);
	if (-1 != fd)
		close(fd);
}

int
xmlbool(const XML_Char *s)
{

	return(0 == strcasecmp(s, "1") || 0 == strcasecmp(s, "true"));
}

void
xmlstrtext(char **p, size_t *sz, const XML_Char *s, int len)
{
	size_t		 ssz;

	if (len > 0) {
		ssz = *sz;
		*sz += (size_t)len;
		*p = xrealloc(*p, *sz + 1);
		memcpy(*p + ssz, s, len);
		(*p)[*sz] = '\0';
	}
}

void
xmlstrclose(char **p, size_t *sz, const XML_Char *name)
{
	size_t		 ssz;

	if (xmlvoid(name))
		return;

	ssz = strlen(name) + 3;
	*sz += ssz;
	*p = xrealloc(*p, *sz + 1);
	strlcat(*p, "</", *sz + 1);
	strlcat(*p, name, *sz + 1);
	strlcat(*p, ">", *sz + 1);
}

void
xmlstropen(char **p, size_t *sz, 
	const XML_Char *name, const XML_Char **atts)
{
	size_t		 ssz;
	int		 isvoid;

	isvoid = xmlvoid(name);

	ssz = strlen(name) + 2 + isvoid;
	*sz += ssz;
	*p = xrealloc(*p, *sz + 1);
	strlcat(*p, "<", *sz + 1);
	strlcat(*p, name, *sz + 1);

	for ( ; NULL != *atts; atts += 2) {
		ssz = strlen(atts[0]) + 2;
		*sz += ssz;
		*p = xrealloc(*p, *sz + 1);
		strlcat(*p, " ", *sz + 1);
		strlcat(*p, atts[0], *sz + 1);
		strlcat(*p, "=", *sz + 1);
		ssz = strlen(atts[1]) + 2;
		*sz += ssz;
		*p = xrealloc(*p, *sz + 1);
		strlcat(*p, "\"", *sz + 1);
		strlcat(*p, atts[1], *sz + 1);
		strlcat(*p, "\"", *sz + 1);
	}

	if (isvoid)
		strlcat(*p, "/", *sz + 1);

	strlcat(*p, ">", *sz + 1);
}

/*
 * Print an XML closure tag unless the element is void.
 * See xmlopen().
 */
void
xmlclose(FILE *f, const XML_Char *name)
{

	if ( ! xmlvoid(name))
		fprintf(f, "</%s>", name);
}

/*
 * Print an XML tag with a varying number of arguments, key then value
 * pairs, terminating with a single NULL.
 * This will auto-close any void elements.
 * See xmlclose().
 */
void
xmlopen(FILE *f, const XML_Char *name, ...)
{
	va_list	 	 ap;
	const XML_Char	*attr;

	fputc('<', f);
	fputs(name, f);

	va_start(ap, name);
	while (NULL != (attr = va_arg(ap, XML_Char *))) {
		fputc(' ', f);
		fputs(attr, f);
		fputs("=\"", f);
		fputs(va_arg(ap, XML_Char *), f);
		fputc('"', f);
	}
	va_end(ap);

	if (xmlvoid(name)) 
		fputs(" /", f);

	fputc('>', f);
}

void
xmlopensx(FILE *f, const XML_Char *s, const XML_Char **atts, 
	const char *url, const struct article *article)
{
	const char	*start, *end, *cp;
	size_t		 sz;

	fputc('<', f);
	fputs(s, f);

	for ( ; NULL != *atts; atts += 2) {
		fputc(' ', f);
		fputs(atts[0], f);
		fputs("=\"", f);
		start = atts[1];
		while (NULL != (cp = strstr(start, "${"))) {
			if (NULL == (end = strchr(cp, '}')))
				break;
			fprintf(f, "%.*s", (int)(cp - start), start);
			start = cp + 2;
			sz = end - start;
			if (sz == 3 && 0 == memcmp(start, "url", sz))
				fputs(url, f);
			start = end + 1;
		}

		fputs(start, f);
		fputc('"', f);
	}
	if (xmlvoid(s))
		fputs(" /", f);
	fputc('>', f);
}

void
xmlopens(FILE *f, const XML_Char *s, const XML_Char **atts)
{

	fputc('<', f);
	fputs(s, f);
	for ( ; NULL != *atts; atts += 2) {
		fputc(' ', f);
		fputs(atts[0], f);
		fputs("=\"", f);
		fputs(atts[1], f);
		fputc('"', f);
	}
	if (xmlvoid(s))
		fputs(" /", f);
	fputc('>', f);
}

void *
xcalloc(size_t nm, size_t sz)
{
	void	*p;

	if (NULL != (p = calloc(nm, sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}

void *
xmalloc(size_t sz)
{
	void	*p;

	if (NULL != (p = malloc(sz)))
		return(p);

	perror(NULL);
	exit(EXIT_FAILURE);
}
