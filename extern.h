/*	$Id$ */
/*
 * Copyright (c) 2013, 2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include "sblg.h"

__BEGIN_DECLS

enum	xmlesc {
	XMLESC_NONE = 0,
	XMLESC_WS = 0x01, /* internal use only */
	XMLESC_ATTR = 0x02,
	XMLESC_HTML = 0x04
};

int	atom(XML_Parser p, const char *templ, int sz, 
		char *src[], const char *dst, enum asort asort);
int	json(XML_Parser p, int sz, 
		char *src[], const char *dst, enum asort asort);
int	listtags(XML_Parser, int, char *[], int, int, int);
int	compile(XML_Parser p, const char *templ,
		const char *src, const char *dst);
int	linkall(XML_Parser p, const char *templ, const char *force, 
		int sz, char *src[], const char *dst, enum asort asort);
int	linkall_r(XML_Parser p, const char *templ, 
		int sz, char *src[], enum asort asort);

void	mmap_close(int fd, void *buf, size_t sz);
int	mmap_open(const char *f, int *fd, char **buf, size_t *sz);

void	xmlstrclose(char **, size_t *, const XML_Char *);
void	xmlstropen(char **, size_t *, const XML_Char *,
		const XML_Char **, const char **);
void	xmlstrtext(char **, size_t *, const XML_Char *, int);

int	xmlbool(const XML_Char *s);
void	xmlclose(FILE *, const XML_Char *);
void	xmlopen(FILE *, const XML_Char *, ...);
void	xmlopens(FILE *, const XML_Char *, const XML_Char **);
void	xmlopensx(FILE *, const XML_Char *, const XML_Char **, 
		const char *, const struct article *, size_t, size_t);
void	xmltextx(FILE *f, const XML_Char *s, const char *, 
		const struct article *, size_t,
		size_t, size_t, size_t, size_t, enum xmlesc);

void	hashtag(char ***, size_t *, const char *,
		const struct article *, size_t, ssize_t);

void	*xcalloc(size_t, size_t);
void	*xmalloc(size_t);
char	*xstrdup(const char *);
char	*xstrndup(const char *, size_t);
void	*xrealloc(void *, size_t);
void	*xreallocarray(void *, size_t, size_t);

__END_DECLS

#endif 
