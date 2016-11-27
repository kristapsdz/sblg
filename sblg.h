/*	$Id$ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>,
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

struct	article {
	const char	 *src; /* source filename */
	char		 *base; /* nil-terminated src w/o suffix */
	char		 *stripbase; /* nil-terminated fname w/o suffix */
	char		 *striplangbase; /* stripbase w/o langs */
	char		 *title; /* nil-terminated title */
	size_t		  titlesz; /* length of title */
	char		 *titletext; /* nil-terminated title text */
	size_t		  titletextsz; /* length of titletext */
	char		 *aside; /* nil-terminated aside content */
	size_t		  asidesz; /* length of aside */
	char		 *asidetext; /* nil-terminated aside text */
	size_t		  asidetextsz; /* length of asidetext */
	char		 *author; /* nil-terminated author name */
	size_t		  authorsz; /* length of author */
	char		 *authortext; /* nil-terminated author name text */
	size_t		  authortextsz; /* length of authortext */
	time_t	 	  time; /* date of publication */
	int		  isdatetime; /* whether the date has a time */
	char		 *article; /* nil-terminated entire article */
	size_t		  articlesz; /* length of article */
	char		**tagmap; /* array of tags */
	size_t		  tagmapsz; /* length of tag array */
	char		 *img; /* image associated with article */
	enum sort	  sort; /* overriden sort order parameters */
};

__BEGIN_DECLS

int	sblg_parse(XML_Parser, const char *, struct article **, size_t *);

void	sblg_free(struct article *, size_t);

__END_DECLS

#endif 
