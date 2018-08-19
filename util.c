/*	$Id$ */
/*
 * Copyright (c) 2013, 2014, 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
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

int
cmdlinecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	return(s1->order - s2->order);
}

int
filenamecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	if (s1->sort != s2->sort) {
		if (SORT_FIRST == s1->sort || 
		    SORT_LAST == s2->sort)
			return(-1);
		else if (SORT_LAST == s1->sort || 
			 SORT_FIRST == s2->sort)
			return(1);
	}

	return(strcmp(s1->src, s2->src));
}

int
rdatecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	if (s1->sort != s2->sort) {
		if (SORT_FIRST == s2->sort || 
		    SORT_LAST == s1->sort)
			return(-1);
		else if (SORT_LAST == s2->sort || 
			 SORT_FIRST == s1->sort)
			return(1);
	}

	return(difftime(s1->time, s2->time));
}

int
datecmp(const void *p1, const void *p2)
{
	const struct article *s1 = p1, *s2 = p2;

	if (s1->sort != s2->sort) {
		if (SORT_FIRST == s1->sort || 
		    SORT_LAST == s2->sort)
			return(-1);
		else if (SORT_LAST == s1->sort || 
			 SORT_FIRST == s2->sort)
			return(1);
	}

	return(difftime(s2->time, s1->time));
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
		warn("%s", f);
		goto out;
	} else if (-1 == fstat(*fd, &st)) {
		warn("%s", f);
		goto out;
	} else if ( ! S_ISREG(st.st_mode)) {
		warnx("%s: not a regular file", f);
		goto out;
	} else if (st.st_size >= (1U << 31)) {
		warnx("%s: too large", f);
		goto out;
	}

	*sz = (size_t)st.st_size;
	*buf = mmap(NULL, *sz, PROT_READ, MAP_FILE|MAP_SHARED, *fd, 0);

	if (MAP_FAILED == *buf) {
		warn("%s", f);
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
xmlstrflush(char *cp, size_t *sz)
{

	if (0 == *sz)
		return;

	*cp = '\0';
	*sz = 0;
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

static void
xmlescape(FILE *f, const char *cp)
{

	for ( ; '\0' != *cp; cp++) 
		switch (*cp) {
		case ('"'):
			fputs("&quot;", f);
			break;
		case ('&'):
			fputs("&amp;", f);
			break;
		default:
			fputc(*cp, f);
			break;
		}
}

static size_t
xmlstrescapesz(const char *cp)
{
	size_t	sz;

	for (sz = 0; '\0' != *cp; cp++) 
		switch (*cp) {
		case ('"'):
			sz += strlen("&quot;");
			break;
		case ('&'):
			sz += strlen("&amp;");
			break;
		default:
			sz++;
			break;
		}

	return(sz);
}

static void
xmlstrescape(char *p, size_t sz, const char *cp)
{
	size_t	ssz;

	ssz = strlen(p);

	for ( ; '\0' != *cp; cp++) 
		switch (*cp) {
		case ('"'):
			ssz = strlcat(p, "&quot;", sz);
			break;
		case ('&'):
			ssz = strlcat(p, "&amp;", sz);
			break;
		default:
			p[ssz++] = *cp;
			p[ssz] = '\0';
			break;
		}
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
	/* Make sure we zero the initial buffer. */
	if (NULL == *p)
		*p = xcalloc(*sz + 1, 1);
	else 
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
		/* 
		 * Remember to buffer in '&quot;' space. 
		 * Use enough as if each word were that.
		 * FIXME: this is totally unnecessary.
		 */
		ssz = xmlstrescapesz(atts[1]) + 2;
		*sz += ssz;
		*p = xrealloc(*p, *sz + 1);
		strlcat(*p, "\"", *sz + 1);
		xmlstrescape(*p, *sz + 1, atts[1]);
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
		xmlescape(f, va_arg(ap, XML_Char *));
		fputc('"', f);
	}
	va_end(ap);

	if (xmlvoid(name)) 
		fputs(" /", f);

	fputc('>', f);
}

/*
 * Format an article's localtime.
 * We accept "auto" or a string that's pumped into strftime(3).
 * If "auto", this determines which to use based upon we were given both
 * a date and a time.
 * The result is meant to be human-readable.
 */
static void
fmttime(char *buf, size_t bufsz, const char *arg, 
	size_t argsz, int isdatetime, struct tm *tm)
{
	char	*fmt;

	if (0 == argsz || 
	    (4 == argsz && 0 == strncmp(arg, "auto", argsz))) 
		fmt = xstrdup(isdatetime ? "%c" : "%x");
	else
		fmt = xstrndup(arg, argsz);

	strftime(buf, bufsz, fmt, tm);
	free(fmt);
}

/*
 * List all tags for article "art".
 * The tag listing appears as a set of <span class"sblg-tag"> elements
 * filled with the tag name (escaped space normalised).
 * If no tags are found, then prints <span class="sblg-tags-notfound">.
 * If given a prefix "arg" of non-zero size "argsz", only tags with the
 * matching case-sensitive prefix are printed.
 */
static void
taglist(FILE *f, const struct article *art, const char *arg, size_t argsz)
{
	size_t	 	 i, sz, found;
	const char	*cp;

	for (found = i = 0; i < art->tagmapsz; i++) {
		if (argsz > 0) {
			sz = strlen(art->tagmap[i]);
			if (sz <= argsz || 
			    strncmp(arg, art->tagmap[i], argsz)) 
				continue;
		}
		fputs("<span class=\"sblg-tag\">", f);
		for (cp = art->tagmap[i] + argsz; '\0' != *cp; cp++) {
			if ('\\' == cp[0] && ' ' == cp[1])
				continue;
			if ('<' == *cp)
				fputs("&lt;", f);
			else if ('>' == *cp)
				fputs("&gt;", f);
			else if ('"' == *cp)
				fputs("&quot;", f);
			else if ('&' == *cp)
				fputs("&amp;", f);
			else
				fputc(*cp, f);
		}
		fputs("</span>", f);
		found = 1;
	}
	if (0 == found)
		fputs("<span class=\"sblg-tags-notfound\"></span>", f);
}

/*
 * Given the nil-terminated string "s", emit all of its characters to
 * "f" while substituting ${sblg-xxxxx} tags in the process.
 * This uses the current array of articles "arts" length "artsz",
 * currently at position "artpos".
 * The "url" is the current file being written (naming "f").
 * FIXME: the contents written are not escaped in any way.
 */
void
xmltextx(FILE *f, const XML_Char *s, const char *url, 
	const struct article *arts, size_t artsz, size_t artpos, 
	size_t realpos)
{
	const char	*cp, *start, *end, *arg, *bufp;
	char		 buf[32];
	size_t		 sz, next, prev, argsz, i, asz;

	if (NULL == s || '\0' == *s)
		return;

	prev = (artpos + 1) % artsz;
	next = artpos == 0 ? artsz - 1 : artpos - 1;

	/*
	 * Check whether "_word", string length "_sz", is the same as
	 * the value currently at "start".
	 */

#define	STRCMP(_word, _sz) \
	(sz == (_sz) && 0 == memcmp(start, (_word), (_sz)))

	start = s;
	while (NULL != (cp = strstr(start, "${"))) {
		if (NULL == (end = strchr(cp, '}')))
			break;
		fprintf(f, "%.*s", (int)(cp - start), start);
		start = cp + 2;
		sz = end - start;
		arg = NULL;
		argsz = 0;
		bufp = "";

		if (NULL != (arg = memchr(start, '|', sz))) {
			sz = arg - start;
			arg++;
			argsz = end - arg;
		} 

		/*
		 * The following tags all dynamically construct a value
		 * that we put into the "bufp" pointer, which defaults
		 * to the empty string.
		 */

		if (STRCMP("sblg-date", 9)) {
			strftime(buf, sizeof(buf), "%Y-%m-%d", 
				gmtime(&arts[artpos].time));
			bufp = buf;
		} else if (STRCMP("sblg-datetime", 13)) {
			strftime(buf, sizeof(buf), "%Y-%m-%dT%TZ", 
				gmtime(&arts[artpos].time));
			bufp = buf;
		} else if (STRCMP("sblg-datetime-fmt", 17)) {
			fmttime(buf, sizeof(buf), arg, argsz,
				arts[artpos].isdatetime,
				localtime(&arts[artpos].time));
			bufp = buf;
		} else if (STRCMP("sblg-pos", 8)) {
			snprintf(buf, sizeof(buf), "%zu", realpos + 1);
			bufp = buf;
		} else if (STRCMP("sblg-abspos", 11)) {
			snprintf(buf, sizeof(buf), "%zu", artpos + 1);
			bufp = buf;
		} else if (STRCMP("sblg-get", 8)) {
			for (i = 0; i < arts[artpos].setmapsz; i += 2) {
				asz = strlen(arts[artpos].setmap[i]);
				/* Ugly and slow, but effective. */
				if (asz == argsz && 0 == memcmp
				    (arts[artpos].setmap[i], 
				     arg, argsz)) {
					bufp = arts[artpos].setmap[i + 1];
					break;
				}
			}
		} else if (STRCMP("sblg-has", 8)) {
			for (i = 0; i < arts[artpos].setmapsz; i += 2) {
				asz = strlen(arts[artpos].setmap[i]);
				/* Ugly and slow, but effective. */
				if (asz == argsz && 0 == memcmp
				    (arts[artpos].setmap[i], 
				     arg, argsz)) {
					bufp = arg;
					break;
				}
			}
		}

		if (STRCMP("sblg-base", 9))
			fputs(arts[artpos].base, f);
		else if (STRCMP("sblg-get", 8))
			fputs(bufp, f);
		else if (STRCMP("sblg-has", 8)) {
			if (NULL != bufp && '\0' != *bufp)
				fprintf(f, "sblg-has-%.*s", (int)argsz, arg);
		} else if (STRCMP("sblg-tags", 9))
			taglist(f, &arts[artpos], arg, argsz);
		else if (STRCMP("sblg-stripbase", 14))
			fputs(arts[artpos].stripbase, f);
		else if (STRCMP("sblg-striplangbase", 18))
			fputs(arts[artpos].striplangbase, f);
		else if (STRCMP("sblg-first-base", 15))
			fputs(arts[0].base, f);
		else if (STRCMP("sblg-first-stripbase", 20))
			fputs(arts[0].stripbase, f);
		else if (STRCMP("sblg-first-striplangbase", 24))
			fputs(arts[0].striplangbase, f);
		else if (STRCMP("sblg-last-base", 14))
			fputs(arts[artsz - 1].base, f);
		else if (STRCMP("sblg-last-stripbase", 19))
			fputs(arts[artsz - 1].stripbase, f);
		else if (STRCMP("sblg-last-striplangbase", 23))
			fputs(arts[artsz - 1].striplangbase, f);
		else if (STRCMP("sblg-next-base", 14))
			fputs(arts[next].base, f);
		else if (STRCMP("sblg-next-stripbase", 19))
			fputs(arts[next].stripbase, f);
		else if (STRCMP("sblg-next-striplangbase", 23))
			fputs(arts[next].striplangbase, f);
		else if (STRCMP("sblg-prev-base", 14))
			fputs(arts[prev].base, f);
		else if (STRCMP("sblg-prev-stripbase", 19))
			fputs(arts[prev].stripbase, f);
		else if (STRCMP("sblg-prev-striplangbase", 23))
			fputs(arts[prev].striplangbase, f);
		else if (STRCMP("sblg-title", 10))
			fputs(arts[artpos].title, f);
		else if (STRCMP("sblg-url", 8))
			fputs(NULL == url ? "" : url, f);
		else if (STRCMP("sblg-titletext", 14))
			fputs(arts[artpos].titletext, f);
		else if (STRCMP("sblg-author", 11))
			fputs(arts[artpos].author, f);
		else if (STRCMP("sblg-authortext", 15))
			fputs(arts[artpos].authortext, f);
		else if (STRCMP("sblg-source", 11))
			fputs(arts[artpos].src, f);
		else if (STRCMP("sblg-date", 9))
			fputs(bufp, f);
		else if (STRCMP("sblg-datetime", 13))
			fputs(bufp, f);
		else if (STRCMP("sblg-datetime-fmt", 17))
			fputs(bufp, f);
		else if (STRCMP("sblg-pos", 8))
			fputs(bufp, f);
		else if (STRCMP("sblg-abspos", 11))
			fputs(bufp, f);
		else if (STRCMP("sblg-aside", 10))
			fputs(arts[artpos].aside, f);
		else if (STRCMP("sblg-asidetext", 14))
			fputs(arts[artpos].asidetext, f);
		else if (STRCMP("sblg-img", 8) &&
		         NULL != arts[artpos].img)
			fputs(arts[artpos].img, f);

		start = end + 1;
	}

	fputs(start, f);
}

/*
 * Emits to "f" an XML element named "s" with NULL-terminated argument
 * list "atts" of key-value string pairs (libexpat style).
 * Passes all of the attribute values through xmltextx().
 * See xmltextx() for an explanation of the remaining variables.
 */
void
xmlopensx(FILE *f, const XML_Char *s, 
	const XML_Char **atts, const char *url, 
	const struct article *art, size_t artsz, size_t artpos)
{

	fputc('<', f);
	fputs(s, f);

	for ( ; NULL != *atts; atts += 2) {
		fputc(' ', f);
		fputs(atts[0], f);
		fputs("=\"", f);
		xmltextx(f, atts[1], url, art, artsz, artpos, 0);
		fputc('"', f);
	}
	if (xmlvoid(s))
		fputs(" /", f);
	fputc('>', f);
}

/*
 * Open an XML element named "s" with NULL-terminated argument list
 * "atts" of key-value string pairs (libexpat style).
 */
void
xmlopens(FILE *f, const XML_Char *s, const XML_Char **atts)
{

	fputc('<', f);
	fputs(s, f);
	for ( ; NULL != *atts; atts += 2) {
		fputc(' ', f);
		fputs(atts[0], f);
		fputs("=\"", f);
		xmlescape(f, atts[1]);
		fputc('"', f);
	}
	if (xmlvoid(s))
		fputs(" /", f);
	fputc('>', f);
}

/*
 * Wrapper for strndup(3).
 * Exits on memory allocation failure.
 */
char *
xstrndup(const char *cp, size_t sz)
{
	void	*p;

	if (NULL == (p = strndup(cp, sz)))
		err(EXIT_FAILURE, NULL);
	return(p);
}

/*
 * Wrapper for strdup(3).
 * Exits on memory allocation failure.
 */
char *
xstrdup(const char *cp)
{
	void	*p;

	if (NULL == (p = strdup(cp)))
		err(EXIT_FAILURE, NULL);
	return(p);
}

/*
 * Wrapper for reallocarray(3).
 * Exits on memory allocation failure.
 */
void *
xreallocarray(void *cp, size_t nm, size_t sz)
{
	void	*p;

	if (NULL == (p = reallocarray(cp, nm, sz)))
		err(EXIT_FAILURE, NULL);
	return(p);
}

/*
 * Wrapper for realloc(3).
 * Exits on memory allocation failure.
 */
void *
xrealloc(void *cp, size_t sz)
{
	void	*p;

	if (NULL == (p = realloc(cp, sz)))
		err(EXIT_FAILURE, NULL);
	return(p);
}

/*
 * Wrapper for calloc(3).
 * Exits on memory allocation failure.
 */
void *
xcalloc(size_t nm, size_t sz)
{
	void	*p;

	if (NULL == (p = calloc(nm, sz)))
		err(EXIT_FAILURE, NULL);
	return(p);
}

/*
 * Wrapper for malloc(3).
 * Exits on memory allocation failure.
 */
void *
xmalloc(size_t sz)
{
	void	*p;

	if (NULL == (p = malloc(sz)))
		err(EXIT_FAILURE, NULL);
	return(p);
}

void
hashset(char ***map, size_t *sz, const char *key, const char *val)
{
	size_t	 i;

	assert(NULL != key && '\0' != *key);

	for (i = 0; i < *sz; i += 2)
		if (0 == strcmp(key, (*map)[i]))
			break;

	if (i < *sz) {
		free((*map)[i + 1]);
		(*map)[i + 1] = xstrdup(val);
		return;
	}

	*map = xreallocarray(*map, *sz + 2, sizeof(char *));
	(*map)[*sz] = xstrdup(key);
	(*map)[*sz + 1] = xstrdup(val);
	(*sz) += 2;
}

/*
 * Break apart "in" into navigation tags "map" of size "in".
 * The input is a string of space-separated except for those with
 * backslash-escaped spaces.
 * Use this for data-sblg-navtags or data-sblg-tag.
 */
void
hashtag(char ***map, size_t *sz, const char *in,
	const struct article *arts, size_t artsz, ssize_t artpos)
{
	char	*start, *end, *cur, *tofree, *astart, *aend;
	size_t	 i;
	int	 rc;

	if ('\0' == in[0])
		return;

	tofree = cur = xstrdup(in);

	for (;;) {
		/* Skip past leading whitespace. */
		while (' ' == cur[0])
			cur++;
		if ('\0' == cur[0])
			break;

		/* Parse word til next non-escaped whitespace. */
		for (start = end = cur; '\0' != end[0]; end++)
			if (' ' == end[0] && '\\' != end[-1])
				break;

		/* Set us at the next token. */
		cur = end;
		if ('\0' != *end)
			cur++;
		*end = '\0';

		/* Search for duplicates. */
		for (i = 0; i < *sz; i++)
			if (0 == strcmp((*map)[i], start))
				break;

		if (i < *sz)
			continue;

		*map = xreallocarray(*map, *sz + 1, sizeof(char *));

		/*
		 * We allow "sblg-get" invocations within these strings
		 * IFF "arts" is non-NULL.
		 */

		if (NULL == arts || artpos < 0 ||
		    NULL == (astart = strstr(start, "${sblg-get|")) ||
		    NULL == (aend = strchr(astart + 11, '}'))) {
			(*map)[*sz] = xstrdup(start);
			(*sz)++;
			continue;
		} 

		/* Terminate leading matter, terminate key. */

		*astart = '\0';
		astart += 11;
		*aend++ = '\0';

		/* Look up in setter map. */

		for (i = 0; i < arts[artpos].setmapsz; i += 2) {
			if (strcmp(arts[artpos].setmap[i], astart))
				continue;
			rc = asprintf(&(*map)[*sz], "%s%s%s", start,
				arts[artpos].setmap[i + 1], aend);
			if (rc < 0)
				err(EXIT_FAILURE, NULL);
			break;
		}

		if (i == arts[artpos].setmapsz)
			asprintf(&(*map)[*sz], "%s%s", start, aend);

		(*sz)++;
	}

	free(tofree);
}
