/*	$Id$ */
/*
 * Copyright (c) 2013--2016, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

/*
 * Holds all information on a pending parse of the Atom template.
 */
struct	atom {
	FILE		*f; /* atom template input */
	const char	*src; /* template file */
	const char	*dst; /* output or "-" for stdout */
	XML_Parser	 p; /* parser instance */
	char		 domain[MAXHOSTNAMELEN]; /* ...or localhost */
	char		 path[MAXPATHLEN]; /* path in link */
	struct article	*sargs; /* articles */
	size_t		 spos; /* current article */
	size_t		 sposz; /* article length */
	size_t		 stack; /* position in discard stack */
};

/* Functions may be called out-of-order of definitions. */

static void entry_begin(void *, const XML_Char *, const XML_Char **);
static void entry_empty(void *, const XML_Char *);
static void entry_end(void *, const XML_Char *);
static void id_begin(void *, const XML_Char *, const XML_Char **);
static void id_end(void *, const XML_Char *);
static void tmpl_begin(void *, const XML_Char *, const XML_Char **);
static void tmpl_end(void *, const XML_Char *);
static void tmpl_text(void *, const XML_Char *, int);
static void up_begin(void *, const XML_Char *, const XML_Char **);
static void up_end(void *, const XML_Char *);

static void
atomputs(FILE *f, const char *cp)
{
	
	for ( ; '\0' != *cp; cp++)
		switch (*cp) {
		case ('<'):
			fputs("&lt;", f);
			break;
		case ('>'):
			fputs("&gt;", f);
			break;
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

static void
atomprint(FILE *f, const struct atom *arg, int altlink, 
	int striplink, int content, const struct article *src)
{
	char		 buf[1024];
	struct tm	*tm;

	tm = gmtime(&src->time);

	strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
	fprintf(f, "<id>tag:%s,%s:%s/%s</id>\n", 
		arg->domain, buf, arg->path, src->src);

	strftime(buf, sizeof(buf), "%Y-%m-%dT%TZ", tm);
	fprintf(f, "<updated>%s</updated>\n", buf);

	fprintf(f, "<title>%s</title>\n", src->titletext);
	fprintf(f, "<author><name>%s</name></author>\n", src->authortext);
	if (altlink && ! striplink)
		fprintf(f, "<link rel=\"alternate\" type=\"text/html\" "
			 "href=\"%s/%s\" />\n", arg->path, src->src);
	else if (altlink)
		fprintf(f, "<link rel=\"alternate\" type=\"text/html\" "
			 "href=\"%s/%s\" />\n", arg->path, src->stripsrc);

	if (content && NULL != src->article) {
		fprintf(f, "<content type=\"html\">");
		atomputs(f, src->article);
		fprintf(f, "</content>");
	} else {
		fprintf(f, "<content type=\"html\">");
		atomputs(f, src->aside);
		fprintf(f, "</content>");
	}
}

/*
 * Given the Atom template "templ", fill in entries etc.
 * Return zero on failure, non-zero on success.
 */
int
atom(XML_Parser p, const char *templ, int sz, 
	char *src[], const char *dst, enum asort asort)
{
	char		*buf;
	size_t		 ssz, sargsz;
	int		 i, fd, rc;
	FILE		*f;
	struct atom	 larg;
	struct article	*sargs;

	ssz = 0;
	rc = 0;
	buf = NULL;
	fd = -1;
	f = NULL;

	memset(&larg, 0, sizeof(struct atom));
	sargs = NULL;
	sargsz = 0;

	if (getdomainname(larg.domain, MAXHOSTNAMELEN) < 0) {
		warn("getdomainname");
		goto out;
	} else if ('\0' == larg.domain[0])
		strlcpy(larg.domain, "localhost", MAXHOSTNAMELEN);

	strlcpy(larg.path, "/", MAXPATHLEN);

	for (i = 0; i < sz; i++)
		if ( ! sblg_parse(p, src[i], &sargs, &sargsz))
			goto out;

	if (ASORT_DATE == asort)
		qsort(sargs, sargsz, sizeof(struct article), datecmp);
	else if (ASORT_RDATE == asort)
		qsort(sargs, sargsz, sizeof(struct article), rdatecmp);
	else if (ASORT_FILENAME == asort)
		qsort(sargs, sargsz, sizeof(struct article), filenamecmp);

	f = stdout;
	if (strcmp(dst, "-") && NULL == (f = fopen(dst, "w"))) {
		warn("%s", dst);
		goto out;
	}

	if ( ! mmap_open(templ, &fd, &buf, &ssz))
		goto out;

	larg.sargs = sargs;
	larg.sposz = sargsz;
	larg.p = p;
	larg.src = templ;
	larg.dst = dst;
	larg.f = f;

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, tmpl_text);
	XML_SetElementHandler(p, tmpl_begin, tmpl_end);
	XML_SetUserData(p, &larg);

	if (XML_STATUS_OK != XML_Parse(p, buf, (int)ssz, 1)) {
		warnx("%s:%zu:%zu: %s", templ, 
			XML_GetCurrentLineNumber(p),
			XML_GetCurrentColumnNumber(p),
			XML_ErrorString(XML_GetErrorCode(p)));
		goto out;
	} 

	fputc('\n', f);
	rc = 1;
out:
	sblg_free(sargs, sargsz);
	mmap_close(fd, buf, ssz);
	if (NULL != f && stdout != f)
		fclose(f);
	return(rc);
}

static void
tmpl_text(void *userdata, const XML_Char *s, int len)
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
up_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct atom	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "updated");
}

static void
id_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct atom	*arg = userdata;

	arg->stack += 0 == strcasecmp(name, "id");
}

static void
tmpl_begin(void *userdata, 
	const XML_Char *name, const XML_Char **atts)
{
	struct atom	 *arg = userdata;
	time_t		  t;
	char		  buf[1024];
	int		  altlink, content, striplink, useall = 0;
	const char	 *start;
	char		 *cp;
	struct tm	 *tm;
	const XML_Char	**attp;

	assert(0 == arg->stack);

	if (0 == strcasecmp(name, "updated")) {
		for (attp = atts; NULL != *attp; attp += 2)
			if (0 == strcasecmp(*attp, "data-sblg-updated"))
				break;
		if (NULL == *attp || ! xmlbool(attp[1])) {
			xmlopens(arg->f, name, atts);
			return;
		}
		fprintf(arg->f, "<%s>", name);
		t = arg->sposz <= arg->spos ?
			time(NULL) :
			arg->sargs[arg->spos].time;
		tm = gmtime(&t);
		strftime(buf, sizeof(buf), "%Y-%m-%dT%TZ", tm);
		fprintf(arg->f, "%s", buf);
		arg->stack++;
		XML_SetDefaultHandlerExpand(arg->p, NULL);
		XML_SetElementHandler(arg->p, up_begin, up_end);
		return;
	} else if (0 == strcasecmp(name, "id")) {
		for (attp = atts; NULL != *attp; attp += 2)
			if (0 == strcasecmp(*attp, "data-sblg-id"))
				break;
		if (NULL == *attp || ! xmlbool(attp[1])) {
			xmlopens(arg->f, name, atts);
			return;
		}
		fprintf(arg->f, "<%s>", name);
		arg->stack++;
		XML_SetDefaultHandlerExpand(arg->p, NULL);
		XML_SetElementHandler(arg->p, id_begin, id_end);
		return;
	} else if (0 == strcasecmp(name, "link")) {
		if (arg->spos > 0) {
			warnx("%s: link appears"
				"after entry", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}
		xmlopens(arg->f, name, atts);
		for (attp = atts; NULL != *attp; attp += 2) 
			if (0 == strcasecmp(attp[0], "rel"))
				if (0 == strcasecmp(attp[1], "self"))
					break;
		if (NULL == *attp)
			return;
		for (attp = atts; NULL != *attp; attp += 2) 
			if (0 == strcasecmp(attp[0], "href"))
				break;
		if (NULL == *attp) {
			warnx("%s: no href", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}
		if (NULL == (start = strcasestr(attp[1], "://"))) {
			warnx("%s: bad uri", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}
		strlcpy(arg->domain, start + 3, MAXHOSTNAMELEN);
		if (NULL == (cp = strchr(arg->domain, '/'))) {
			warnx("%s: bad uri", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}
		strlcpy(arg->path, cp, MAXPATHLEN);
		*cp = '\0';
		if (NULL == (cp = strrchr(arg->path, '/'))) {
			warnx("%s: bad uri", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}
		*cp = '\0';
		return;
	} else if (strcasecmp(name, "entry")) {
		xmlopens(arg->f, name, atts);
		return;
	}

	for (attp = atts; NULL != *attp; attp += 2)
		if (0 == strcasecmp(*attp, "data-sblg-entry"))
			break;

	if (NULL == *attp || ! xmlbool(attp[1])) {
		xmlopens(arg->f, name, atts);
		return;
	}

	altlink = 1;
	striplink = content = 0;

	for (attp = atts; NULL != *attp; attp += 2)
		if (0 == strcasecmp(*attp, "data-sblg-altlink"))
			altlink = xmlbool(attp[1]);
		else if (0 == strcasecmp(*attp, "data-sblg-striplink"))
			striplink = xmlbool(attp[1]);
		else if (0 == strcasecmp(*attp, "data-sblg-content"))
			content = xmlbool(attp[1]);
		else if (0 == strcasecmp(*attp, "data-sblg-forall"))
			useall = xmlbool(attp[1]);

	arg->stack++;
	XML_SetDefaultHandlerExpand(arg->p, NULL);
	if (arg->sposz > arg->spos && useall) {
		while (arg->spos < arg->sposz) {
			fprintf(arg->f, "<%s>", name);
			atomprint(arg->f, arg, altlink, striplink,
				content, &arg->sargs[arg->spos++]);
			fprintf(arg->f, "</%s>", name);
		}
		XML_SetDefaultHandlerExpand(arg->p, NULL);
		XML_SetElementHandler(arg->p, entry_begin, entry_end);
	} else if (arg->sposz > arg->spos) {
		fprintf(arg->f, "<%s>", name);
		XML_SetDefaultHandlerExpand(arg->p, NULL);
		XML_SetElementHandler(arg->p, entry_begin, entry_end);
		fprintf(arg->f, "</%s>", name);
	} else {
		XML_SetElementHandler(arg->p, entry_begin, entry_empty);
	}
}

static void
entry_empty(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;

	if (0 == strcasecmp(name, "entry") && 0 == --arg->stack) {
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
	}
}

static void
tmpl_end(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;

	xmlclose(arg->f, name);
}

static void
id_end(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;
	const char	*dst;
	char		 year[32];
	time_t		 t = time(NULL);
	struct tm	*tm;

	if (NULL != (tm = localtime(&t))) {
		snprintf(year, sizeof(year), "%04d", tm->tm_year + 1900);
	} else {
		/* Um... */
		warn("localtime");
		strlcpy(year, "2013", sizeof(year));
	}

	dst = (0 == strcmp(arg->dst, "-")) ? "" : arg->dst;

	if (0 == strcasecmp(name, "id") && 0 == --arg->stack) {
		fprintf(arg->f, "tag:%s,%s:%s/%s</%s>", 
			arg->domain, year, arg->path, dst, name);
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
	}
}

static void
up_end(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;

	if (0 == strcasecmp(name, "updated") && 0 == --arg->stack) {
		fprintf(arg->f, "</%s>", name);
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
	}
}

static void
entry_end(void *userdata, const XML_Char *name)
{
	struct atom	*arg = userdata;

	if (0 == strcasecmp(name, "entry") && 0 ==  --arg->stack) {
		XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
		XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
	}
}
