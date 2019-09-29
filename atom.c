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
	XML_Parser	 p; /* parser instance */
	char		*id; /* the main identifier (or NULL) */
	size_t		 idsz; /* length of id or zero */
	char		*link; /* first <link> (or NULL) */
	struct article	*sargs; /* articles */
	size_t		 spos; /* current article */
	size_t		 sposz; /* article length */
	size_t		 stack; /* position in discard stack */
	int		 entryfl; /* flags for current entry */
#define ENTRY_STRIP	 0x01 /* TODO: deprecate */
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
	int		      idsz;
	const struct article *src;

	/*
	 * If we have data-sblg-atomcontent, we do the same as in
	 * navcontent and replace the contents while looking up tags.
	 * Don't escape this in any way: we can use (for example) XML
	 * namespaces to control for having HTML5 content.
	 */

	if ((arg->entryfl & ENTRY_REPL)) {
		xmltextx(arg->f, arg->entry, "atom.xml",
			arg->sargs, arg->sposz, arg->sposz, 
			arg->spos, arg->spos, arg->sposz, XMLESC_NONE);
		return;
	}

	/* Create an atom <entry> from the data we have. */

	src = &arg->sargs[arg->spos];
	tm = gmtime(&src->time);
	strftime(buf, sizeof(buf), "%Y-%m-%dT%TZ", tm);
	idsz = (arg->idsz && arg->id[arg->idsz - 1] == '/') ?
		arg->idsz - 1 : arg->idsz;

	fprintf(arg->f, "\t\t<id>%.*s/%s#%s</id>\n", 
		idsz, arg->id, src->src, buf);
	fprintf(arg->f, "\t\t<updated>%s</updated>\n", buf);
	fprintf(arg->f, "\t\t<title>%s</title>\n", src->titletext);
	fprintf(arg->f, "\t\t<author><name>%s</name></author>\n", 
		src->authortext);

	if ((arg->entryfl & ENTRY_ALT)) {
		if (arg->entryalt != NULL) {
			fputs("\t\t<link rel=\"alternate\" type="
				"\"text/html\" " "href=\"", arg->f);
			xmltextx(arg->f, arg->entryalt, "atom.xml", 
				arg->sargs, arg->sposz, arg->sposz, 
				arg->spos, arg->spos, arg->sposz, 
				XMLESC_ATTR);
			fputs("\" />\n", arg->f);
		} else if (!(arg->entryfl & ENTRY_STRIP)) {
			fprintf(arg->f, "\t\t<link rel=\"alternate\" "
				"type=\"text/html\" href=\"%s\" "
				"/>\n", src->src);
		} else {
			fprintf(arg->f, "\t\t<link rel=\"alternate\" "
				"type=\"text/html\" href=\"%s\" "
				"/>\n", src->stripsrc);
		}
	}
	
	/*
	 * If we have data-sblg-atomcontent, then replace the content
	 * within the <entry> with our own.
	 * If data-sblg-content (stupid, by the way), then simply inline
	 * all of our content.
	 */

	fputs("\t\t<content type=\"xhtml\">\n"
	      "\t\t\t<div xmlns=\"http://www.w3.org/1999/xhtml\">\n", arg->f);
	if ((arg->entryfl & ENTRY_CONTENT)) {
		xmltextx(arg->f, src->article, "atom.xml", 
			arg->sargs, arg->sposz, arg->sposz, arg->spos, 
			arg->spos, arg->sposz, XMLESC_NONE);
	} else {
		xmltextx(arg->f, src->aside, "atom.xml",
			arg->sargs, arg->sposz, arg->sposz, arg->spos, 
			arg->spos, arg->sposz, XMLESC_NONE);
	}
	fputs("</div>\n"
	      "\t\t</content>\n", arg->f);
}

/*
 * Given the Atom template "templ", fill in entries etc.
 * Return zero on failure, non-zero on success.
 */
int
atom(XML_Parser p, const char *templ, int sz, 
	char *src[], const char *dst, enum asort asort)
{
	char		*buf = NULL;
	size_t		 ssz = 0, sargsz = 0;
	int		 i, fd = -1, rc = 0;
	FILE		*f = stdout;
	struct atom	 larg;
	struct article	*sargs = NULL;

	memset(&larg, 0, sizeof(struct atom));

	for (i = 0; i < sz; i++)
		if (!sblg_parse(p, src[i], &sargs, &sargsz, atom_wl))
			goto out;

	sblg_sort(sargs, sargsz, asort);

	if (strcmp(dst, "-") && (f = fopen(dst, "w")) == NULL) {
		warn("%s", dst);
		goto out;
	}

	if (!mmap_open(templ, &fd, &buf, &ssz))
		goto out;

	larg.sargs = sargs;
	larg.sposz = sargsz;
	larg.p = p;
	larg.src = templ;
	larg.f = f;

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, tmpl_text);
	XML_SetElementHandler(p, tmpl_begin, tmpl_end);
	XML_SetUserData(p, &larg);

	if (XML_Parse(p, buf, (int)ssz, 1) != XML_STATUS_OK) {
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
	if (f != NULL && f != stdout)
		fclose(f);
	free(larg.entry);
	free(larg.entryalt);
	return rc;
}

static void
tmpl_text(void *dat, const XML_Char *s, int len)
{
	struct atom	*arg = dat;

	fprintf(arg->f, "%.*s", len, s);
}

static void
entry_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct atom	*arg = dat;

	arg->stack += (sblg_lookup(s) == SBLG_ELEM_ENTRY);
	xmlstropen(&arg->entry, &arg->entrysz, s, atts, NULL);
}

static void
up_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct atom	*arg = dat;

	warnx("%s: no elements allowed in <updated>", arg->src);
	XML_StopParser(arg->p, 0);
}

static void
id_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct atom	*arg = dat;

	warnx("%s: no elements allowed in <id>", arg->src);
	XML_StopParser(arg->p, 0);
}

static void
id_text(void *dat, const XML_Char *s, int len)
{
	struct atom	*arg = dat;

	xmlstrtext(&arg->id, &arg->idsz, s, len);
	fprintf(arg->f, "%.*s", len, s);
}


static void
tmpl_begin(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct atom	 *arg = dat;
	time_t		  t;
	char		  buf[64];
	struct tm	 *tm;
	const XML_Char	**attp, *attsv;
	size_t		  i;
	enum sblgtag	  tag;

	assert(0 == arg->stack);

	switch (sblg_lookup(s)) {
	case SBLG_ELEM_UPDATED:
		fputs("<updated>", arg->f);

		/*
		 * For <updated>, use the newest article's time.
		 * Use the current time if there is none.
		 * Discard anything already in the element.
		 * FIXME: will sorting (-s) affect this?
		 * FIXME: keep contents in case there are custom <entry>
		 * fields.
		 */

		t = arg->sposz <= arg->spos ?
			time(NULL) : arg->sargs[arg->spos].time;
		tm = gmtime(&t);
		strftime(buf, sizeof(buf), "%Y-%m-%dT%TZ", tm);
		fputs(buf, arg->f);
		XML_SetDefaultHandlerExpand(arg->p, NULL);
		XML_SetElementHandler(arg->p, up_begin, up_end);
		return;
	case SBLG_ELEM_ID:
		fputs("<id>", arg->f);

		/*
		 * For <id>, either fill in with our <link> href if none
		 * is provided, otherwise use what's already in the
		 * identifier.
		 */

		free(arg->id);
		arg->id = NULL;
		arg->idsz = 0;
		XML_SetDefaultHandlerExpand(arg->p, id_text);
		XML_SetElementHandler(arg->p, id_begin, id_end);
		return;
	case SBLG_ELEM_LINK:
		for (attp = atts; *attp != NULL; attp += 2) 
			if (strcmp(attp[0], "href") == 0)
				break;
		if (*attp == NULL) {
			warnx("%s: no href", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}
		attsv = attp[1];

		/*
		 * We use the <link> element to grab the URI of the
		 * alternate content.  We'll (optionally) use this as
		 * the <id> value for feed elements, so don't validate
		 * it until we actually need to.
		 */

		xmlopens(arg->f, s, atts);
		for (attp = atts; *attp != NULL; attp += 2) 
			if (strcmp(attp[0], "rel") == 0 &&
		   	    strcmp(attp[1], "alternate") != 0)
				break;
		if (*attp == NULL) {
			free(arg->link);
			arg->link = xstrdup(attsv);
		}
		return;
	case SBLG_ELEM_ENTRY:
		break;
	default:
		xmlopens(arg->f, s, atts);
		return;
	}

	/* Drive the <entry data-sblg-entry="1"> logic. */

	for (attp = atts; *attp != NULL; attp += 2)
		if (sblg_lookup(*attp) == SBLG_ATTR_ENTRY)
			break;
	if (*attp == NULL || !xmlbool(attp[1])) {
		xmlopens(arg->f, s, atts);
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
			if (tag != entryattrs[i].tag)
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
tmpl_end(void *userdata, const XML_Char *s)
{
	struct atom	*arg = userdata;

	xmlclose(arg->f, s);
}

/*
 * Emit an identifier iff one is not provided.
 * If one is not provided, use the <link> element, requiring that it be
 * a full URL and provide a trailing slash if one doesn't appear.
 */
static void
id_end(void *dat, const XML_Char *s)
{
	struct atom	*arg = dat;
	const char	*cp = NULL;

	if (arg->id == NULL) {
		assert(arg->idsz == 0);
		if (arg->link == NULL) {
			warnx("%s: need at least <link> "
				"to create <id>", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}

		if (strncmp(arg->link, "http://", 7) == 0)
			cp = arg->link + 7;
		else if (strncmp(arg->link, "https://", 8) == 0)
			cp = arg->link + 8;

		if (cp == NULL) {
			warnx("%s: need absolute <link> URL "
				"to create <id>", arg->src);
			XML_StopParser(arg->p, 0);
			return;
		}

		if (strchr(cp, '/') != NULL)
			arg->id = xstrdup(arg->link);
		else if (asprintf(&arg->id, "%s/", arg->link) == -1)
			err(EXIT_FAILURE, "asprintf");

		arg->idsz = strlen(arg->id);
		fputs(arg->id, arg->f);
	}

	fputs("</id>", arg->f);
	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
}

static void
up_end(void *dat, const XML_Char *s)
{
	struct atom	*arg = dat;

	fputs("</updated>", arg->f);
	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	XML_SetDefaultHandlerExpand(arg->p, tmpl_text);
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
	int		 first = 1;

	if (!(sblg_lookup(s) == SBLG_ELEM_ENTRY && --arg->stack == 0)) {
		xmlstrclose(&arg->entry, &arg->entrysz, s);
		return;
	}

	/* Terminal <entry>, so do post-processing. */

	XML_SetElementHandler(arg->p, tmpl_begin, tmpl_end);
	XML_SetDefaultHandlerExpand(arg->p, tmpl_text);

	/* No more articles: discard. */

	if (arg->spos < arg->sposz) {
		if ((arg->entryfl & ENTRY_FORALL)) {
			while (arg->spos < arg->sposz) {
				if (!first)
					fputs("\t", arg->f);
				fputs("<entry>\n", arg->f);
				atomprint(arg);
				fputs("\t</entry>\n", arg->f);
				arg->spos++;
				first = 0;
			}
		} else {
			fputs("<entry>\n", arg->f);
			atomprint(arg);
			fputs("\t</entry>\n", arg->f);
			arg->spos++;
		}
	}

	free(arg->entry);
	free(arg->entryalt);
	arg->entry = arg->entryalt = NULL;
	arg->entrysz = 0;
}
