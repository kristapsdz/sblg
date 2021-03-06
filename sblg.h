/*	$Id$ */
/*
 * Copyright (c) 2016--2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef SBLG_H
#define SBLG_H

enum	sort {
	SORT_DEFAULT = 0,
	SORT_FIRST,
	SORT_LAST
};

enum	asort {
	ASORT_DATE = 0,
	ASORT_FILENAME,
	ASORT_CMDLINE,
	ASORT_ITITLE,
	ASORT_TITLE,
	ASORT_RDATE,
	ASORT_RFILENAME,
	ASORT_RCMDLINE,
	ASORT_RITITLE,
	ASORT_RTITLE
};

enum	sblgtag {
	SBLG_ATTR_ALTLINK,
	SBLG_ATTR_ALTLINKFMT,
	SBLG_ATTR_ARTICLE,
	SBLG_ATTR_ARTICLETAG,
	SBLG_ATTR_ASIDE,
	SBLG_ATTR_ATOMCONTENT,
	SBLG_ATTR_AUTHOR,
	SBLG_ATTR_CONST_ASIDE,
	SBLG_ATTR_CONST_AUTHOR,
	SBLG_ATTR_CONST_DATETIME,
	SBLG_ATTR_CONST_IMG,
	SBLG_ATTR_CONST_TITLE,
	SBLG_ATTR_CONTENT,
	SBLG_ATTR_DATETIME,
	SBLG_ATTR_ENTRY,
	SBLG_ATTR_FORALL,
	SBLG_ATTR_IGN_ONCE,
	SBLG_ATTR_IMG,
	SBLG_ATTR_LANG,
	SBLG_ATTR_NAV,
	SBLG_ATTR_NAVCONTENT,
	SBLG_ATTR_NAVSORT,
	SBLG_ATTR_NAVSTART,
	SBLG_ATTR_NAVSZ,
	SBLG_ATTR_NAVTAG,
	SBLG_ATTR_NAVXML,
	SBLG_ATTR_PERMLINK,
	SBLG_ATTR_SORT,
	SBLG_ATTR_SOURCE,
	SBLG_ATTR_STRIPLINK,
	SBLG_ATTR_TAGS,
	SBLG_ATTR_TITLE,
	SBLG_ELEM_ADDRESS,
	SBLG_ELEM_AREA,
	SBLG_ELEM_ARTICLE,
	SBLG_ELEM_ASIDE,
	SBLG_ELEM_BASE,
	SBLG_ELEM_BR,
	SBLG_ELEM_COL,
	SBLG_ELEM_COMMAND,
	SBLG_ELEM_EMBED,
	SBLG_ELEM_ENTRY,
	SBLG_ELEM_H1,
	SBLG_ELEM_H2,
	SBLG_ELEM_H3,
	SBLG_ELEM_H4,
	SBLG_ELEM_HR,
	SBLG_ELEM_ID,
	SBLG_ELEM_IMG,
	SBLG_ELEM_INPUT,
	SBLG_ELEM_KEYGEN,
	SBLG_ELEM_LINK,
	SBLG_ELEM_META,
	SBLG_ELEM_NAV,
	SBLG_ELEM_PARAM,
	SBLG_ELEM_SOURCE,
	SBLG_ELEM_TIME,
	SBLG_ELEM_TITLE,
	SBLG_ELEM_TRACK,
	SBLG_ELEM_UPDATED,
	SBLG_ELEM_WBR,
	SBLGTAG_NONE
};

/*
 * All strings are NUL-terminated.
 */
struct	article {
	char	 	 *real; /* real filename */
	char	 	 *stripreal; /* real filename w/o directory */
	char		 *realbase; /* real w/o suffix */
	char		 *striprealbase; /* realbase w/o suffix */
	char		 *striplangrealbase; /* striprealbase w/o langs */
	char	 	 *src; /* source filename */
	char	 	 *stripsrc; /* source filename w/o directory */
	char		 *base; /* src w/o suffix */
	char		 *stripbase; /* base w/o suffix */
	char		 *striplangbase; /* stripbase w/o langs */
	char		 *title; /* title */
	size_t		  titlesz; /* length of title */
	char		 *titletext; /* title text */
	size_t		  titletextsz; /* length of titletext */
	char		 *aside; /* aside content */
	size_t		  asidesz; /* length of aside */
	char		 *asidetext; /* aside text */
	size_t		  asidetextsz; /* length of asidetext */
	char		 *author; /* author name */
	size_t		  authorsz; /* length of author */
	char		 *authortext; /* author name text */
	size_t		  authortextsz; /* length of authortext */
	time_t	 	  time; /* date of publication */
	int		  isdatetime; /* whether the date has a time */
	char		 *article; /* entire article */
	size_t		  articlesz; /* length of article */
	char		**tagmap; /* array of tags */
	size_t		  tagmapsz; /* length of tag array */
	char		**setmap; /* array of key-value custom keys */
	size_t		  setmapsz; /* both keys and vals of setmap */
	char		 *img; /* image associated with article */
	enum sort	  sort; /* overriden sort order parameters */
	size_t		  order; /* cmdline sort order */
};

__BEGIN_DECLS

int		sblg_init(void);
void		sblg_destroy(void);
enum sblgtag	sblg_lookup(const char *);

int		sblg_parse(XML_Parser, const char *,
			struct article **, size_t *, const char **);
void		sblg_free(struct article *, size_t);
void		sblg_sort(struct article *, size_t, enum asort);
int		sblg_sort_lookup(const char *, enum asort *);

__END_DECLS

#endif 
