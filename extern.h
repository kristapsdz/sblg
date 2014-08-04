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
	char		*aside; /* nil-terminated aside content */
	size_t		 asidesz; /* length of aside */
	char		*author; /* nil-terminated author name */
	size_t		 authorsz; /* length of author */
	time_t	 	 time; /* date of publication */
	char		*article; /* nil-terminated entire article */
	size_t		 articlesz; /* length of article */
	char		*tags; /* any article tags */
};

#define	xrealloc realloc
#define	xcalloc	calloc
#define	xmalloc	malloc
#define	xstrdup	strdup

int	atom(XML_Parser p, const char *templ,
		int sz, char *src[], const char *dst);
int	compile(XML_Parser p, const char *templ,
		const char *src, const char *dst);
int	echo(FILE *f, int linked, const char *src);
int	grok(XML_Parser p, const char *src, struct article *data);
void	grok_free(struct article *p);
void	mmap_close(int fd, void *buf, size_t sz);
int	mmap_open(const char *f, int *fd, char **buf, size_t *sz);
int	linkall(XML_Parser p, const char *templ, const char *force, 
		int sz, char *src[], const char *dst);
void	xmlappend(char **p, size_t *sz, 
		const XML_Char *s, int len);
int	xmlbool(const XML_Char *s);
void	xmlprint(FILE *f, const XML_Char *s, const XML_Char **atts);
void	xmlrappendclose(char **p, size_t *sz, const XML_Char *name);
void	xmlrappendopen(char **p, size_t *sz, 
		const XML_Char *name, const XML_Char **atts);
int	xmlvoid(const XML_Char *s);

__END_DECLS

#endif 
