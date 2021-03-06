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
#include <search.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"
#include "version.h"

struct	htab {
	char		*tabent; /* used in the table entry */
	const char	*name; /* the static name */
	enum sblgtag	 tag; /* static tag */
};

static	int htabinit = 0;

static	struct htab htabs[SBLGTAG_NONE] = {
	{ NULL, "data-sblg-altlink", SBLG_ATTR_ALTLINK },
	{ NULL, "data-sblg-altlink-fmt", SBLG_ATTR_ALTLINKFMT },
	{ NULL, "data-sblg-article", SBLG_ATTR_ARTICLE },
	{ NULL, "data-sblg-articletag", SBLG_ATTR_ARTICLETAG },
	{ NULL, "data-sblg-aside", SBLG_ATTR_ASIDE },
	{ NULL, "data-sblg-atomcontent", SBLG_ATTR_ATOMCONTENT },
	{ NULL, "data-sblg-author", SBLG_ATTR_AUTHOR },
	{ NULL, "data-sblg-const-aside", SBLG_ATTR_CONST_ASIDE },
	{ NULL, "data-sblg-const-author", SBLG_ATTR_CONST_AUTHOR },
	{ NULL, "data-sblg-const-datetime", SBLG_ATTR_CONST_DATETIME },
	{ NULL, "data-sblg-const-img", SBLG_ATTR_CONST_IMG },
	{ NULL, "data-sblg-const-title", SBLG_ATTR_CONST_TITLE },
	{ NULL, "data-sblg-content", SBLG_ATTR_CONTENT },
	{ NULL, "data-sblg-datetime", SBLG_ATTR_DATETIME },
	{ NULL, "data-sblg-entry", SBLG_ATTR_ENTRY },
	{ NULL, "data-sblg-forall", SBLG_ATTR_FORALL },
	{ NULL, "data-sblg-ign-once", SBLG_ATTR_IGN_ONCE },
	{ NULL, "data-sblg-img", SBLG_ATTR_IMG },
	{ NULL, "data-sblg-lang", SBLG_ATTR_LANG },
	{ NULL, "data-sblg-nav", SBLG_ATTR_NAV },
	{ NULL, "data-sblg-navcontent", SBLG_ATTR_NAVCONTENT },
	{ NULL, "data-sblg-navsort", SBLG_ATTR_NAVSORT },
	{ NULL, "data-sblg-navstart", SBLG_ATTR_NAVSTART },
	{ NULL, "data-sblg-navsz", SBLG_ATTR_NAVSZ },
	{ NULL, "data-sblg-navtag", SBLG_ATTR_NAVTAG },
	{ NULL, "data-sblg-navxml", SBLG_ATTR_NAVXML },
	{ NULL, "data-sblg-permlink", SBLG_ATTR_PERMLINK },
	{ NULL, "data-sblg-sort", SBLG_ATTR_SORT },
	{ NULL, "data-sblg-source", SBLG_ATTR_SOURCE },
	{ NULL, "data-sblg-striplink", SBLG_ATTR_STRIPLINK },
	{ NULL, "data-sblg-tags", SBLG_ATTR_TAGS },
	{ NULL, "data-sblg-title", SBLG_ATTR_TITLE },
	{ NULL, "address", SBLG_ELEM_ADDRESS },
	{ NULL, "area", SBLG_ELEM_AREA },
	{ NULL, "article", SBLG_ELEM_ARTICLE },
	{ NULL, "aside", SBLG_ELEM_ASIDE },
	{ NULL, "base", SBLG_ELEM_BASE },
	{ NULL, "br", SBLG_ELEM_BR },
	{ NULL, "col", SBLG_ELEM_COL },
	{ NULL, "command", SBLG_ELEM_COMMAND },
	{ NULL, "embed", SBLG_ELEM_EMBED },
	{ NULL, "entry", SBLG_ELEM_ENTRY },
	{ NULL, "h1", SBLG_ELEM_H1 },
	{ NULL, "h2", SBLG_ELEM_H2 },
	{ NULL, "h3", SBLG_ELEM_H3 },
	{ NULL, "h4", SBLG_ELEM_H4 },
	{ NULL, "hr", SBLG_ELEM_HR },
	{ NULL, "id", SBLG_ELEM_ID },
	{ NULL, "img", SBLG_ELEM_IMG },
	{ NULL, "input", SBLG_ELEM_INPUT },
	{ NULL, "keygen", SBLG_ELEM_KEYGEN },
	{ NULL, "link", SBLG_ELEM_LINK },
	{ NULL, "meta", SBLG_ELEM_META },
	{ NULL, "nav", SBLG_ELEM_NAV },
	{ NULL, "param", SBLG_ELEM_PARAM },
	{ NULL, "source", SBLG_ELEM_SOURCE },
	{ NULL, "time", SBLG_ELEM_TIME },
	{ NULL, "title", SBLG_ELEM_TITLE },
	{ NULL, "track", SBLG_ELEM_TRACK },
	{ NULL, "updated", SBLG_ELEM_UPDATED },
	{ NULL, "wbr", SBLG_ELEM_WBR },
};

/*
 * All possible self-closing (e.g., <area />) HTML5 elements.
 * These are defined in section 8.1.2 of the HTML 5.2 standard.
 * It also has some deprecated/obsolete elements.
 */
static	const enum sblgtag elemvoid[] = {
	SBLG_ELEM_AREA,
	SBLG_ELEM_BASE,
	SBLG_ELEM_BR,
	SBLG_ELEM_COL,
	SBLG_ELEM_COMMAND, /* XXX: obsolete. */
	SBLG_ELEM_EMBED,
	SBLG_ELEM_HR,
	SBLG_ELEM_IMG,
	SBLG_ELEM_INPUT,
	SBLG_ELEM_KEYGEN, /* XXX: deprecated. */
	SBLG_ELEM_LINK,
	SBLG_ELEM_META,
	SBLG_ELEM_PARAM,
	SBLG_ELEM_SOURCE,
	SBLG_ELEM_TRACK,
	SBLG_ELEM_WBR,
	SBLGTAG_NONE
};

/*
 * Look up a given HTML5 element to see if it can be "void", i.e., it
 * cannot have any content and closes itself out.
 * For example, <p> is not void; <link /> is.
 * Returns zero on failure, non-zero on found.
 */
static int
htmlvoid(const XML_Char *s)
{
	enum sblgtag	 tag;
	size_t		 i;

	if ((tag = sblg_lookup(s)) == SBLGTAG_NONE)
		return 0;
	for (i = 0; elemvoid[i] != SBLGTAG_NONE; i++)
		if (elemvoid[i] == tag)
			return 1;

	return 0;
}

/*
 * Look up the given HTML5 attribute in a whitelist.
 * Returns zero on failure, non-zero on found.
 */
static int
htmlwhitelist(const XML_Char *s, const char **wl)
{
	const char	**cp;

	for (cp = wl; *cp != NULL; cp++) 
		if (strcasecmp(s, *cp) == 0)
			return 1;

	return 0;
}

/*
 * Map a regular file into memory for parsing.
 * Make sure it's not too large, first.
 * Return zero on failure, non-zero on success.
 * On success, symmetrise with mmap_close().
 * Failure need not call mmap_close().
 */
int
mmap_open(const char *f, int *fd, char **buf, size_t *sz)
{
	struct stat	 st;

	*fd = -1;
	*buf = NULL;
	*sz = 0;

	if ((*fd = open(f, O_RDONLY, 0)) == -1) {
		warn("%s", f);
		goto out;
	} else if (fstat(*fd, &st) == -1) {
		warn("%s", f);
		goto out;
	} else if (!S_ISREG(st.st_mode)) {
		warnx("%s: not a regular file", f);
		goto out;
	} else if (st.st_size >= (1U << 31)) {
		warnx("%s: too large", f);
		goto out;
	}

	*sz = (size_t)st.st_size;
	*buf = mmap(NULL, *sz, PROT_READ, MAP_SHARED, *fd, 0);

	if (*buf == MAP_FAILED) {
		warn("%s", f);
		goto out;
	}

	return 1;
out:
	mmap_close(*fd, *buf, *sz);
	return 0;
}

/*
 * Reverse of mmap_open, though can be called with NULL/invalid.
 * Do NOT call twice.
 */
void
mmap_close(int fd, void *buf, size_t sz)
{

	if (buf != NULL)
		munmap(buf, sz);
	if (fd != -1)
		close(fd);
}

/*
 * Whether an XML attribute value evaluates to Boolean true.
 * Return zero on false, non-zero on true.
 */
int
xmlbool(const XML_Char *s)
{

	assert(s != NULL);
	return strcasecmp(s, "1") == 0 || strcasecmp(s, "true") == 0;
}

/*
 * Augment the string "p" of length "sz" with the given data.
 * The string is NUL-terminated.
 */
void
xmlstrtext(char **p, size_t *sz, const XML_Char *s, int len)
{
	size_t	 ssz;

	if (len > 0) {
		ssz = *sz;
		*sz += (size_t)len;
		*p = xrealloc(*p, *sz + 1);
		memcpy(*p + ssz, s, len);
		(*p)[*sz] = '\0';
	}
}

/*
 * Augment the string "p" of length "sz" with the closing tag.
 * The string is NUL-terminated.
 * This doesn't append a closing tag for void elements.
 */
void
xmlstrclose(char **p, size_t *sz, const XML_Char *name)
{
	size_t	 ssz;

	if (htmlvoid(name))
		return;

	ssz = strlen(name) + 3;
	*sz += ssz;
	*p = xrealloc(*p, *sz + 1);
	strlcat(*p, "</", *sz + 1);
	strlcat(*p, name, *sz + 1);
	strlcat(*p, ">", *sz + 1);
}

/*
 * Print an XML attribute string "cp", escaping the contents so that it
 * doesn't invalidate the attribute scope (quotes).
 * FIXME: use strcspn() instead of iterating.
 */
static void
xmlescape(FILE *f, const char *cp)
{

	for ( ; *cp != '\0'; cp++) 
		switch (*cp) {
		case  '"':
			fputs("&quot;", f);
			break;
		case  '&':
			fputs("&amp;", f);
			break;
		default:
			fputc(*cp, f);
			break;
		}
}

/*
 * Like xmlescape(), but only getting the size of the number of
 * characters we'd otherwise write.
 * Returns the length of the escaped string, which may be zero.
 * FIXME: use strcspn().
 */
static size_t
xmlstrescapesz(const char *cp)
{
	size_t	sz;

	for (sz = 0; *cp != '\0'; cp++) 
		switch (*cp) {
		case '"':
			sz += strlen("&quot;");
			break;
		case '&':
			sz += strlen("&amp;");
			break;
		default:
			sz++;
			break;
		}

	return sz;
}

/*
 * Like xmlescape(), but serialising into a string.
 * The buffer must be preallocated like with xmlstrescapesz().
 * FIXME: use strcspn().
 */
static void
xmlstrescape(char *p, size_t sz, const char *cp)
{
	size_t	ssz;

	ssz = strlen(p);

	for ( ; *cp != '\0'; cp++) 
		switch (*cp) {
		case '"':
			ssz = strlcat(p, "&quot;", sz);
			break;
		case '&':
			ssz = strlcat(p, "&amp;", sz);
			break;
		default:
			p[ssz++] = *cp;
			p[ssz] = '\0';
			break;
		}
}

/*
 * Append the XML element "name" opening to the buffer in "p", which is
 * always NUL terminated and of length "sz".
 * If the element is an XML void element, it is auto-closed.
 * If the white-list is not NULL, it is scanned and only attributes in
 * the white-list are serialised.
 */
void
xmlstropen(char **p, size_t *sz, const XML_Char *name,
	const XML_Char **atts, const char **whitelist)
{
	size_t	 ssz;
	int	 isvoid;

	isvoid = htmlvoid(name);

	ssz = strlen(name) + 2 + isvoid;
	*sz += ssz;

	/* Make sure we zero the initial buffer. */

	*p = (*p == NULL) ?
		xcalloc(*sz + 1, 1) : xrealloc(*p, *sz + 1);

	strlcat(*p, "<", *sz + 1);
	strlcat(*p, name, *sz + 1);

	for ( ; *atts != NULL; atts += 2) {
		if (whitelist != NULL &&
		    !htmlwhitelist(atts[0], whitelist))
			continue;
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

	if (!htmlvoid(name))
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
	while ((attr = va_arg(ap, XML_Char *)) != NULL) {
		fputc(' ', f);
		fputs(attr, f);
		fputs("=\"", f);
		xmlescape(f, va_arg(ap, XML_Char *));
		fputc('"', f);
	}
	va_end(ap);

	if (htmlvoid(name)) 
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

	if (argsz == 0 || 
	    (argsz == 4 && strncmp(arg, "auto", argsz) == 0)) 
		fmt = xstrdup(isdatetime ? "%c" : "%x");
	else
		fmt = xstrndup(arg, argsz);

	strftime(buf, bufsz, fmt, tm);
	free(fmt);
}

/*
 * Emit the non-NUL terminated buffer with the given escape type.
 * FIXME: use strcspn to avoid calling into stdio.
 */
static void
xmltextxesc(FILE *f, const char *p, size_t sz, enum xmlesc esc)
{
	size_t	 i;

	if (sz == 0)
		return;

	switch (esc) {
	case XMLESC_NONE:
		fwrite(p, sz, 1, f);
		break;
	case XMLESC_ATTR:
		for (i = 0; i < sz; i++)
			if ((esc & XMLESC_WS) && p[i] == ' ')
				fputs("\\ ", f);
			else if (p[i] == '"')
				fputs("&quot;", f);
			else
				fputc(p[i], f);
		break;
	case XMLESC_HTML:
		for (i = 0; i < sz; i++)
			if ((esc & XMLESC_WS) && p[i] == ' ')
				fputs("\\ ", f);
			else if (p[i] == '<')
				fputs("&lt;", f);
			else if (p[i] == '>')
				fputs("&gt;", f);
			else if (p[i] == '"')
				fputs("&quot;", f);
			else if (p[i] == '&')
				fputs("&amp;", f);
			else
				fputc(p[i], f);
		break;
	case XMLESC_WS:
		for (i = 0; i < sz; i++)
			if (p[i] == ' ')
				fputs("\\ ", f);
			else
				fputc(p[i], f);
		break;
	}
}

/*
 * Like xmltextxesc, but for NUL-terminated strings.
 */
static void
xmltextxescs(FILE *f, const char *p, enum xmlesc esc)
{

	return xmltextxesc(f, p, strlen(p), esc);
}

/*
 * List all tags for article "art".
 * The tag listing appears as a set of <span class"sblg-tag"> elements
 * filled with the tag name (escaped space normalised).
 * If no tags are found, then prints <span class="sblg-tags-notfound">.
 * If given a prefix "arg" of non-zero size "argsz", only tags with the
 * matching case-sensitive prefix are printed.
 * FIXME: use strcspn for speed.
 */
static void
xmltextxtag(FILE *f, const struct article *art,
	const char *arg, size_t argsz, enum xmlesc esc)
{
	size_t	 	 i, sz;
	int		 found = 0;
	const char	*cp;

	for (i = 0; i < art->tagmapsz; i++) {
		if (argsz > 0) {
			sz = strlen(art->tagmap[i]);
			if (sz <= argsz || 
			    strncmp(arg, art->tagmap[i], argsz)) 
				continue;
		}

		xmltextxescs(f, "<span class=\"sblg-tag\">", esc);
		for (cp = art->tagmap[i] + argsz; *cp != '\0'; cp++) {
			if (cp[0] == '\\' && cp[1] == ' ')
				continue;
			if (*cp == '<')
				xmltextxescs(f, "&lt;", esc);
			else if (*cp == '>')
				xmltextxescs(f, "&gt;", esc);
			else if (*cp == '"')
				xmltextxescs(f, "&quot;", esc);
			else if (*cp == '&')
				xmltextxescs(f, "&amp;", esc);
			else
				xmltextxesc(f, cp, 1, esc);
		}
		xmltextxescs(f, "</span>", esc);
		found = 1;
	}
	if (!found)
		xmltextxescs(f, "<span class=\""
			"sblg-tags-notfound\"></span>", esc);
}

/*
 * Emit the NUL-terminated string "s" to "f" while substituting
 * ${sblg-xxxxx} tags in the process.
 * Uses the array of articles "arts" total length "artsz", currently at
 * position "artpos".
 * The "setsz" is set to the number of elements in a tagged set, limited
 * to being called from a navigation having navtags.
 * Otherwise, it's an alias for "artsz".
 * "Realpos" is the position in the shown articles (some may not be
 * shown due to tags), with total shown amount "realsz".
 * The "url" is the current file being written (naming "f").
 * Escape all output using "esc".
 */
void
xmltextx(FILE *f, const XML_Char *s, const char *url, 
	const struct article *arts, size_t setsz, size_t artsz, 
	size_t artpos, size_t realpos, size_t realsz, enum xmlesc esc)
{
	const char	*cp, *start, *end, *arg, *bufp;
	char		 buf[32];
	size_t		 sz, next, prev, argsz, i, asz;

	assert(realsz > 0);

	if (s == NULL || *s == '\0')
		return;

	prev = (artpos + 1) % artsz;
	next = (artpos == 0) ? artsz - 1 : artpos - 1;

	/*
	 * Check whether "_word", string length "_sz", is the same as
	 * the value currently at "start".
	 */

#define	STRCMP(_word, _sz) \
	(sz == (_sz) && memcmp(start, (_word), (_sz)) == 0)

	start = s;
	while ((cp = strstr(start, "${")) != NULL) {
		if ((end = strchr(cp, '}')) == NULL)
			break;

		xmltextxesc(f, start, cp - start, esc);
		start = cp + 2;
		sz = end - start;
		arg = NULL;
		argsz = 0;
		bufp = "";

		if ((arg = memchr(start, '|', sz)) != NULL) {
			sz = arg - start;
			arg++;
			argsz = end - arg;
		} 

		/*
		 * The following tags all dynamically construct a value
		 * that we put into the "bufp" pointer, which defaults
		 * to the empty string.
		 * Most of these just convert numbers using the static
		 * buffer "buf" as staging memory.
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
		} else if (STRCMP("sblg-get", 8) ||
			   STRCMP("sblg-get-escaped", 16) ||
			   STRCMP("sblg-has", 8)) {
			for (i = 0; i < arts[artpos].setmapsz; i += 2) {
				asz = strlen(arts[artpos].setmap[i]);
				/* FIXME: slow but effective. */
				if (asz == argsz && memcmp
				    (arts[artpos].setmap[i], 
				     arg, argsz) == 0) {
					bufp = arts[artpos].setmap[i + 1];
					break;
				}
			}
		} else if (STRCMP("sblg-pos", 8)) {
			snprintf(buf, sizeof(buf), "%zu", realpos + 1);
			bufp = buf;
		} else if (STRCMP("sblg-pos-pct", 12)) {
			snprintf(buf, sizeof(buf), "%.0f", 
				100.0 * (realpos + 1) / realsz);
			bufp = buf;
		} else if (STRCMP("sblg-count", 10)) {
			snprintf(buf, sizeof(buf), "%zu", realsz);
			bufp = buf;
		} else if (STRCMP("sblg-setcount", 13)) {
			snprintf(buf, sizeof(buf), "%zu", setsz);
			bufp = buf;
		} else if (STRCMP("sblg-pos-frac", 13)) {
			snprintf(buf, sizeof(buf), "%.3f", 
				(realpos + 1) / (float)realsz);
			bufp = buf;
		} else if (STRCMP("sblg-abspos", 11)) {
			snprintf(buf, sizeof(buf), "%zu", artpos + 1);
			bufp = buf;
		} else if (STRCMP("sblg-abscount", 13)) {
			snprintf(buf, sizeof(buf), "%zu", artsz);
			bufp = buf;
		}

		if (STRCMP("sblg-base", 9))
			xmltextxescs(f, arts[artpos].base, esc);
		else if (STRCMP("sblg-realbase", 13))
			xmltextxescs(f, arts[artpos].realbase, esc);
		else if (STRCMP("sblg-get", 8))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-get-escaped", 16))
			xmltextxescs(f, bufp, XMLESC_WS | esc);
		else if (STRCMP("sblg-has", 8)) {
			if (bufp != NULL && *bufp != '\0')
				fprintf(f, "sblg-has-%.*s", (int)argsz, arg);
		} else if (STRCMP("sblg-tags", 9))
			xmltextxtag(f, &arts[artpos], arg, argsz, esc);
		else if (STRCMP("sblg-stripbase", 14))
			xmltextxescs(f, arts[artpos].stripbase, esc);
		else if (STRCMP("sblg-striprealbase", 18))
			xmltextxescs(f, arts[artpos].striprealbase, esc);
		else if (STRCMP("sblg-striplangbase", 18))
			xmltextxescs(f, arts[artpos].striplangbase, esc);
		else if (STRCMP("sblg-striplangrealbase", 22))
			xmltextxescs(f, arts[artpos].striplangrealbase, esc);
		else if (STRCMP("sblg-first-base", 15))
			xmltextxescs(f, arts[0].base, esc);
		else if (STRCMP("sblg-first-stripbase", 20))
			xmltextxescs(f, arts[0].stripbase, esc);
		else if (STRCMP("sblg-first-striplangbase", 24))
			xmltextxescs(f, arts[0].striplangbase, esc);
		else if (STRCMP("sblg-last-base", 14))
			xmltextxescs(f, arts[artsz - 1].base, esc);
		else if (STRCMP("sblg-last-stripbase", 19))
			xmltextxescs(f, arts[artsz - 1].stripbase, esc);
		else if (STRCMP("sblg-last-striplangbase", 23))
			xmltextxescs(f, arts[artsz - 1].striplangbase, esc);
		else if (STRCMP("sblg-next-base", 14))
			xmltextxescs(f, arts[next].base, esc);
		else if (STRCMP("sblg-next-has", 13))
			fprintf(f, "%s", artpos ? "sblg-next-has" : "");
		else if (STRCMP("sblg-next-stripbase", 19))
			xmltextxescs(f, arts[next].stripbase, esc);
		else if (STRCMP("sblg-next-striplangbase", 23))
			xmltextxescs(f, arts[next].striplangbase, esc);
		else if (STRCMP("sblg-prev-base", 14))
			xmltextxescs(f, arts[prev].base, esc);
		else if (STRCMP("sblg-prev-has", 13))
			fprintf(f, "%s", artpos < artsz - 1 ? 
				"sblg-prev-has" : "");
		else if (STRCMP("sblg-prev-stripbase", 19))
			xmltextxescs(f, arts[prev].stripbase, esc);
		else if (STRCMP("sblg-prev-striplangbase", 23))
			xmltextxescs(f, arts[prev].striplangbase, esc);
		else if (STRCMP("sblg-title", 10))
			xmltextxescs(f, arts[artpos].title, esc);
		else if (STRCMP("sblg-url", 8))
			xmltextxescs(f, (url == NULL) ? "" : url, esc);
		else if (STRCMP("sblg-titletext", 14))
			xmltextxescs(f, arts[artpos].titletext, esc);
		else if (STRCMP("sblg-author", 11))
			xmltextxescs(f, arts[artpos].author, esc);
		else if (STRCMP("sblg-authortext", 15))
			xmltextxescs(f, arts[artpos].authortext, esc);
		else if (STRCMP("sblg-source", 11))
			xmltextxescs(f, arts[artpos].src, esc);
		else if (STRCMP("sblg-real", 9))
			xmltextxescs(f, arts[artpos].real, esc);
		else if (STRCMP("sblg-date", 9))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-datetime", 13))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-datetime-fmt", 17))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-pos", 8))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-pos-pct", 12))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-count", 10))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-setcount", 13))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-pos-frac", 13))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-abspos", 11))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-abscount", 13))
			xmltextxescs(f, bufp, esc);
		else if (STRCMP("sblg-aside", 10))
			xmltextxescs(f, arts[artpos].aside, esc);
		else if (STRCMP("sblg-asidetext", 14))
			xmltextxescs(f, arts[artpos].asidetext, esc);
		else if (STRCMP("sblg-img", 8))
			xmltextxescs(f, (arts[artpos].img == NULL) ?
				"" : arts[artpos].img, esc);
		else if (STRCMP("sblg-version", 12))
			fputs(VERSION, f);

		start = end + 1;
	}

	xmltextxescs(f, start, esc);
}

/*
 * Emits to "f" an XML element named "s" with NULL-terminated argument
 * list "atts" of key-value string pairs (libexpat style).
 * Passes all of the attribute values through xmltextx().
 * See xmltextx() for an explanation of the remaining parameters.
 */
void
xmlopensx(FILE *f, const XML_Char *s, 
	const XML_Char **atts, const char *url, 
	const struct article *art, size_t artsz, size_t artpos)
{

	fputc('<', f);
	fputs(s, f);

	for ( ; *atts != NULL; atts += 2) {
		fputc(' ', f);
		fputs(atts[0], f);
		fputs("=\"", f);
		xmltextx(f, atts[1], url, art, artsz, 
			artsz, artpos, artsz, artsz, XMLESC_ATTR);
		fputc('"', f);
	}
	if (htmlvoid(s))
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
	for ( ; *atts != NULL; atts += 2) {
		if (strcmp(atts[0], "data-sblg-ign-once") == 0 &&
		    xmlbool(atts[1]))
			continue;
		fputc(' ', f);
		fputs(atts[0], f);
		fputs("=\"", f);
		xmlescape(f, atts[1]);
		fputc('"', f);
	}
	if (htmlvoid(s))
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

	if ((p = strndup(cp, sz)) == NULL)
		err(EXIT_FAILURE, NULL);
	return p;
}

/*
 * Wrapper for strdup(3).
 * Exits on memory allocation failure.
 */
char *
xstrdup(const char *cp)
{
	void	*p;

	if ((p = strdup(cp)) == NULL)
		err(EXIT_FAILURE, NULL);
	return p;
}

/*
 * Wrapper for reallocarray(3).
 * Exits on memory allocation failure.
 */
void *
xreallocarray(void *cp, size_t nm, size_t sz)
{
	void	*p;

	if ((p = reallocarray(cp, nm, sz)) == NULL)
		err(EXIT_FAILURE, NULL);
	return p;
}

/*
 * Wrapper for realloc(3).
 * Exits on memory allocation failure.
 */
void *
xrealloc(void *cp, size_t sz)
{
	void	*p;

	if ((p = realloc(cp, sz)) == NULL)
		err(EXIT_FAILURE, NULL);
	return p;
}

/*
 * Wrapper for calloc(3).
 * Exits on memory allocation failure.
 */
void *
xcalloc(size_t nm, size_t sz)
{
	void	*p;

	if ((p = calloc(nm, sz)) == NULL)
		err(EXIT_FAILURE, NULL);
	return p;
}

/*
 * Wrapper for malloc(3).
 * Exits on memory allocation failure.
 */
void *
xmalloc(size_t sz)
{
	void	*p;

	if ((p = malloc(sz)) == NULL)
		err(EXIT_FAILURE, NULL);
	return p;
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

	if (in[0] == '\0')
		return;

	tofree = cur = xstrdup(in);

	for (;;) {
		/* Skip past leading whitespace. */

		while (cur[0] == ' ')
			cur++;
		if (cur[0] == '\0')
			break;

		/* Parse word til next non-escaped whitespace. */

		for (start = end = cur; end[0] != '\0'; end++)
			if (end[0] == ' ' && end[-1] != '\\')
				break;

		/* Set us at the next token. */

		cur = end;
		if (*end != '\0')
			cur++;
		*end = '\0';

		/* Search for duplicates. */
		for (i = 0; i < *sz; i++)
			if (strcmp((*map)[i], start) == 0)
				break;

		if (i < *sz)
			continue;

		*map = xreallocarray(*map, *sz + 1, sizeof(char *));

		/*
		 * We allow "sblg-get" invocations within these strings
		 * IFF "arts" is non-NULL.
		 */

		if (arts == NULL || artpos < 0 ||
		    (astart = strstr(start, "${sblg-get|")) == NULL ||
		    (aend = strchr(astart + 11, '}')) == NULL) {
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

/*
 * Initialise the hashtable used by sblg to look up attributes.
 * This speeds up the system because we need to look at each attribute
 * to see whether it's one of those we recognise.
 */
int
sblg_init(void)
{
	ENTRY	 hent;
	ENTRY	*hentp;
	size_t	 i;

	assert(!htabinit);

	if (!hcreate(256))
		return 0;

	for (i = 0; i < SBLGTAG_NONE; i++) {
		htabs[i].tabent = xstrdup(htabs[i].name);
		hent.key = htabs[i].tabent;
		hent.data = (void *)&htabs[i].tag;
		if ((hentp = hsearch(hent, ENTER)) == NULL) {
			sblg_destroy();
			return 0;
		}
		assert(hentp->key == hent.key);
	}

	htabinit = 1;
	return 1;
}

void
sblg_destroy(void)
{
#ifdef	__linux__
	size_t	 i;

	for (i = 0; i < SBLGTAG_NONE; i++) {
		free(htabs[i].tabent);
		htabs[i].tabent = NULL;
	}
#endif
	if (!htabinit)
		return;
	hdestroy();
	htabinit = 0;
}

enum sblgtag
sblg_lookup(const char *attr)
{
	ENTRY	 hent;
	ENTRY	*hentp;

	hent.key = (char *)attr;
	hent.data = NULL;
	if ((hentp = hsearch(hent, FIND)) == NULL)
		return SBLGTAG_NONE;

	return *(enum sblgtag *)hentp->data;
}
