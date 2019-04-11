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
	ASORT_RDATE,
	ASORT_FILENAME,
	ASORT_CMDLINE
};

enum	sblgtag {
	SBLG_ATTR_ARTICLE,
	SBLG_ATTR_ARTICLETAG,
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
	SBLG_ATTR_TAGS,
	SBLGTAG_NONE
};

/*
 * All strings are NUL-terminated.
 */
struct	article {
	const char	 *src; /* source filename */
	const char	 *stripsrc; /* source filename w/o directory */
	char		 *base; /* src w/o suffix */
	char		 *stripbase; /* fname w/o suffix */
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
enum sblgtag	sblg_lookup(const char *);

int		sblg_parse(XML_Parser, const char *,
			struct article **, size_t *, const char **);
void		sblg_free(struct article *, size_t);
void		sblg_sort(struct article *, size_t, enum asort);

__END_DECLS

#endif 
