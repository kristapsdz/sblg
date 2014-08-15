/*	$Id$ */
/*
 * Copyright (c) 2013 Kristaps Dzonsons <kristaps@bsd.lv>,
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
#ifndef EXTERN_H
#define EXTERN_H

__BEGIN_DECLS

struct	article {
	const char	*src; /* source filename */
	char		*base; /* nil-terminated src w/o suffix */
	char		*title; /* nil-terminated title */
	size_t		 titlesz; /* length of title */
	char		*titletext; /* nil-terminated title text */
	size_t		 titletextsz; /* length of titletext */
	char		*aside; /* nil-terminated aside content */
	size_t		 asidesz; /* length of aside */
	char		*asidetext; /* nil-terminated aside text */
	size_t		 asidetextsz; /* length of asidetext */
	char		*author; /* nil-terminated author name */
	size_t		 authorsz; /* length of author */
	char		*authortext; /* nil-terminated author name text */
	size_t		 authortextsz; /* length of authortext */
	time_t	 	 time; /* date of publication */
	char		*article; /* nil-terminated entire article */
	size_t		 articlesz; /* length of article */
	char		*tags; /* any article tags */
	size_t		 tagsz;
};

#define	xrealloc realloc
#define	xstrdup	strdup

int	atom(XML_Parser p, const char *templ,
		int sz, char *src[], const char *dst);
int	compile(XML_Parser p, const char *templ,
		const char *src, const char *dst);
int	echo(FILE *f, int linked, const char *src);
int	grok(XML_Parser p, const char *src, struct article *data);
int	linkall(XML_Parser p, const char *templ, const char *force, 
		int sz, char *src[], const char *dst);

void	article_free(struct article *p);
void	mmap_close(int fd, void *buf, size_t sz);
int	mmap_open(const char *f, int *fd, char **buf, size_t *sz);

void	xmlstrclose(char **, size_t *, const XML_Char *);
void	xmlstrflush(char *, size_t *);
void	xmlstropen(char **, size_t *, 
		const XML_Char *, const XML_Char **);
void	xmlstrtext(char **, size_t *, const XML_Char *, int);

int	xmlbool(const XML_Char *s);
void	xmlclose(FILE *, const XML_Char *);
void	xmlopen(FILE *, const XML_Char *, ...);
void	xmlopens(FILE *, const XML_Char *, const XML_Char **);
void	xmlopensx(FILE *, const XML_Char *, const XML_Char **, 
		const char *, const struct article *);
void	xmltextx(FILE *f, const XML_Char *s, 
		const char *, const struct article *);

void	*xcalloc(size_t, size_t);
void	*xmalloc(size_t);

__END_DECLS

#endif 
