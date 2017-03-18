/*	$Id$ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <ctype.h>
#include <expat.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * Print article-major ordering.
 * This prints the tags belonging to each article.
 */
static void
dolist(const struct article *sargs, size_t sargsz, int json)
{
	size_t	 	 i, j;
	const char	*cp;

	if ( ! hcreate(256)) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < sargsz; i++) {
		if (json)
			printf("{\"src\": \"%s\", \n"
			       " \"tags\": [", sargs[i].src);
		for (j = 0; j < sargs[i].tagmapsz; j++) {
			if (json && j > 0)
				putchar(',');
			if (json)
				putchar('"');
			for (cp = sargs[i].tagmap[j]; '\0' != *cp; cp++) {
				if ('\\' == cp[0] && ' ' == cp[1])
					continue;
				putchar(*cp);
			}
			if (json)
				putchar('"');
			else
				printf("\t%s\n", sargs[i].src);
		}
		if (json && i < sargsz - 1)
			puts("]},");
		else if (json)
			puts("]}");
	}
}

int
listtags(XML_Parser p, int sz, char *src[], int json, int reverse)
{
	size_t		 sargsz = 0;
	int		 i;
	struct article	*sargs = NULL;

	/* First run the initial parse of all files. */

	for (i = 0; i < sz; i++) 
		if ( ! sblg_parse(p, src[i], &sargs, &sargsz)) {
			sblg_free(sargs, sargsz);
			return(0);
		}

	/* Now actually emit the listings. */

	if (json)
		puts("{[");
	dolist(sargs, sargsz, json);
	if (json)
		puts("]}");

	/* Cleanup and exit. */

	sblg_free(sargs, sargsz);
	return(1);
}

