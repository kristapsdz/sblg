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
 * White-list of element attributes accepted in Atom feed HTML content.
 * Source:
 * https://validator.w3.org/feed/docs/warning/SecurityRiskAttr.html
 */
static	const char *atom_wl[] = {
	"abbr",
	"accept",
	"accept-charset",
	"accesskey",
	"action",
	"align",
	"alt",
	"axis",
	"border",
	"cellpadding",
	"cellspacing",
	"char",
	"charoff",
	"charset",
	"checked",
	"cite",
	"class",
	"clear",
	"cols",
	"colspan",
	"color",
	"compact",
	"coords",
	"datetime",
	"dir",
	"disabled",
	"enctype",
	"for",
	"frame",
	"headers",
	"height",
	"href",
	"hreflang",
	"hspace",
	"id",
	"ismap",
	"label",
	"lang",
	"longdesc",
	"maxlength",
	"media",
	"method",
	"multiple",
	"name",
	"nohref",
	"noshade",
	"nowrap",
	"prompt",
	"readonly",
	"rel",
	"rev",
	"rows",
	"rowspan",
	"rules",
	"scope",
	"selected",
	"shape",
	"size",
	"span",
	"src",
	"srcset",
	"start",
	"summary",
	"tabindex",
	"target",
	"title",
	"type",
	"usemap",
	"valign",
	"value",
	"vspace",
	"width",
	NULL
};

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
	int		 entryfl; /* flags for current entry */
#define ENTRY_STRIP	 0x01 /* FIXME: deprecate */
#define ENTRY_ALT	 0x02 /* use rel="alternate" */
#define ENTRY_CONTENT	 0x04 /* use full article content */
#define ENTRY_FORALL	 0x08 /* use same entry for all */
#define ENTRY_REPL	 0x10 /* use inline to replace <content> */
	char		*entry; /* saved entry contents */
	char		*entryalt; /* saved entry alt link format */
	size_t		 entrysz; /* length of entry */
};

/*
 * Possible boolean attributes for <entry> processing, assuming that the
 * data-sblg-entry="1" has been set.
 */
struct	entryattr {
	enum sblgtag	 tag; /* <entry> attribute */
	int		 bit; /* bit to set if evaluating to true */
};

static	const struct entryattr entryattrs[] = {
	{ SBLG_ATTR_ALTLINK,	 ENTRY_ALT },
	{ SBLG_ATTR_ATOMCONTENT, ENTRY_REPL },
	{ SBLG_ATTR_CONTENT, 	 ENTRY_CONTENT },
	{ SBLG_ATTR_FORALL, 	 ENTRY_FORALL },
	{ SBLG_ATTR_STRIPLINK, 	 ENTRY_STRIP },
	{ SBLGTAG_NONE, 	 0 }
};

/* Functions may be called out-of-order of definitions. */

static void entry_begin(void *, const XML_Char *, const XML_Char **);
static void entry_end(void *, const XML_Char *);
static void entry_text(void *, const XML_Char *, int);
static void id_begin(void *, const XML_Char *, const XML_Char **);
static void id_end(void *, const XML_Char *);
static void tmpl_begin(void *, const XML_Char *, const XML_Char **);
static void tmpl_end(void *, const XML_Char *);
static void tmpl_text(void *, const XML_Char *, int);
static void up_begin(void *, const XML_Char *, const XML_Char **);
static void up_end(void *, const XML_Char *);

static void
atomprint(const struct atom *arg)
{
	char		      buf[1024];
	struct tm	     *tm;
	const struct article *src;

	src = &arg->sargs[arg->spos];
	tm = gmtime(&src->time);

	strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
	fprintf(arg->f, "<id>tag:%s,%s:%s/%s</id>\n", 
		arg->domain, buf, arg->path, src->src);

	strftime(buf, sizeof(buf), "%Y-%m-%dT%TZ", tm);
	fprintf(arg->f, "<updated>%s</updated>\n", buf);

	fprintf(arg->f, "<title>%s</title>\n", src->titletext);
	fprintf(arg->f, "<author><name>%s</name></author>\n", 
		src->authortext);

	if ((arg->entryfl & ENTRY_ALT)) {
		if (arg->entryalt != NULL) {
			fputs("<link rel=\"alternate\" type="
				"\"text/html\" " "href=\"", arg->f);
			xmltextx(arg->f, arg->entryalt, "atom.xml", 
				arg->sargs, arg->sposz, arg->spos, 
				arg->spos, arg->sposz, XMLESC_ATTR);
			fputs("\" />\n", arg->f);
		} else if ( ! (arg->entryfl & ENTRY_STRIP)) {
			fprintf(arg->f, "<link rel=\"alternate\" "
				"type=\"text/html\" href=\"%s/%s\" "
				"/>\n", arg->path, src->src);
		} else {
			fprintf(arg->f, "<link rel=\"alternate\" "
				"type=\"text/html\" href=\"%s/%s\" "
				"/>\n", arg->path, src->stripsrc);
		}
	}
	
	/*
	 * If we have data-sblg-atomcontent, then replace the content
	 * within the <entry> with our own.
	 * If data-sblg-content (stupid, by the way), then simply inline
	 * all of our content.
	 */

	fputs( "<content type=\"html\">", arg->f);
	if ((arg->entryfl & ENTRY_REPL)) {
		xmltextx(arg->f, arg->entry, "atom.xml", 
			arg->sargs, arg->sposz, arg->spos, 
			arg->spos, arg->sposz, XMLESC_HTML);
	} else if ((arg->entryfl & ENTRY_CONTENT)) {
		xmltextx(arg->f, src->article, "atom.xml", 
			arg->sargs, arg->sposz, arg->spos, 
			arg->spos, arg->sposz, XMLESC_HTML);
	} else {
		xmltextx(arg->f, src->aside, "atom.xml",
			arg->sargs, arg->sposz, arg->spos, 
			arg->spos, arg->sposz, XMLESC_HTML);
	}
	fputs("</content>\n", arg->f);
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
		if ( ! sblg_parse(p, src[i], &sargs, &sargsz, atom_wl))
			goto out;

	sblg_sort(sargs, sargsz, asort);

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
	free(larg.entry);
	free(larg.entryalt);
	return(rc);
}

static void
tmpl_text(void *userdata, const XML_Char *s, int len)
{
	struct atom	*arg = userdata;

	fprintf(arg->f, "%.*s", len, s);
}

static void
entry_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct atom	*arg = dat;

	arg->stack += (0 == strcasecmp(s, "entry"));
	xmlstropen(&arg->entry, &arg->entrysz, s, atts, NULL);
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
	const char	 *start;
	char		 *cp;
	struct tm	 *tm;
	const XML_Char	**attp;
	size_t		  i;
	enum sblgtag	  tag;

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
			warnx("%s: link after entry", arg->src);
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

	/* Drive the <entry data-sblg-entry="1"> logic. */

	for (attp = atts; NULL != *attp; attp += 2)
		if (0 == strcasecmp(attp[0], "data-sblg-entry"))
			break;

	if (NULL == attp[0] || ! xmlbool(attp[1])) {
		xmlopens(arg->f, name, atts);
		return;
	}

	/*
	 * Put all Boolean data-sblg-entry attributes into a bitfield in
	 * the entry field, which we'll post-process in entry_end().
	 */

	arg->entryfl = 0;
	arg->stack++;

	for (attp = atts; *attp != NULL; attp += 2) {
		tag = sblg_lookup(*attp);
		for (i = 0; entryattrs[i].tag != SBLGTAG_NONE; i++) {
			if (tag == entryattrs[i].tag)
				continue;
			if (!xmlbool(attp[1]))
				continue;
			arg->entryfl |= entryattrs[i].bit;
			break;
		}
		if (tag == SBLG_ATTR_ALTLINKFMT) {
			free(arg->entryalt);
			arg->entryalt = xstrdup(attp[1]);
		}
	}

	XML_SetDefaultHandlerExpand(arg->p, entry_text);
	XML_SetElementHandler(arg->p, entry_begin, entry_end);
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

	if (NULL == (tm = localtime(&t))) {
		warn("localtime");
		strlcpy(year, "2013", sizeof(year));
	} else
		snprintf(year, sizeof(year), "%04d", tm->tm_year + 1900);

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
entry_text(void *dat, const XML_Char *s, int len)
{
	struct atom	*arg = dat;

	xmlstrtext(&arg->entry, &arg->entrysz, s, len);
}

static void
entry_end(void *dat, const XML_Char *s)
{
	struct atom	*arg = dat;

	if ( ! (0 == strcasecmp(s, "entry") && 0 == --arg->stack)) {
		xmlstrclose(&arg->entry, &arg->entrysz, s);
		return;
	}

	/* Terminal <entry>, so do post-processing. */

	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	XML_SetDefaultHandlerExpand(arg->p, tmpl_text);

	/* No more articles: discard. */

	if (arg->spos < arg->sposz) {
		if (ENTRY_FORALL & arg->entryfl) {
			while (arg->spos < arg->sposz) {
				fputs("<entry>\n", arg->f);
				atomprint(arg);
				fputs("</entry>\n", arg->f);
				arg->spos++;
			}
		} else {
			fputs("<entry>\n", arg->f);
			atomprint(arg);
			fputs("</entry>\n", arg->f);
			arg->spos++;
		}
	}

	free(arg->entry);
	free(arg->entryalt);
	arg->entry = arg->entryalt = NULL;
	arg->entrysz = 0;
}
