/*	$Id$ */
/*
 * Copyright (c) 2013--2017, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_ERR
# include <err.h>
#endif
#include <expat.h>
#include <getopt.h>
#include <locale.h>
#if HAVE_SANDBOX_INIT
# include <sandbox.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if HAVE_PLEDGE
# include <unistd.h> /* pledge */
#endif

#include "extern.h"
#include "version.h"

/*
 * Types of operations we handle.
 */
enum	op {
	OP_ATOM, /* generate atom feed */
	OP_COMPILE, /* standalone article */
	OP_BLOG, /* amalgamation */
	OP_LISTTAGS, /* list all tags */
	OP_LINK_INPLACE /* amalgamation (multiple in/out) */
};

int
main(int argc, char *argv[])
{
	int		 ch, i, rc, fmtjson = 0, rev = 0, lf = 0;
	const char	*templ = NULL, *outfile = NULL, *force = NULL;
	enum op		 op = OP_BLOG;
	enum asort	 asort = ASORT_DATE;
	XML_Parser	 p;

#if HAVE_SANDBOX_INIT
	if (sandbox_init
	    (kSBXProfileNoNetwork, SANDBOX_NAMED, NULL) < 0)
		errx(EXIT_FAILURE, "sandbox_init");
#endif

#if HAVE_PLEDGE
	if (pledge("stdio cpath rpath wpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	setlocale(LC_ALL, "");

	while (-1 != (ch = getopt(argc, argv, "acjlLrC:o:s:t:V")))
		switch (ch) {
		case 'a':
			op = OP_ATOM;
			break;
		case 'c':
			op = OP_COMPILE;
			break;
		case 'C':
			force = optarg;
			break;
		case 'j':
			fmtjson = 1;
			break;
		case 'l':
			if (op == OP_LISTTAGS)
				lf = 1;
			else
				op = OP_LISTTAGS;
			break;
		case 'L':
			op = OP_LINK_INPLACE;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'r':
			rev = 1;
			break;
		case 's':
			if (!sblg_sort_lookup(optarg, &asort))
				goto usage;
			break;
		case 't':
			templ = optarg;
			break;
		case 'V':
			fputs("sblg-" VERSION "\n", stderr);
			return EXIT_SUCCESS;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		goto usage;

	if (op == OP_BLOG && fmtjson)
		op = OP_ATOM;

	if (!sblg_init())
		err(EXIT_FAILURE, NULL);

	/*
	 * Avoid constantly re-using a parser by specifying one here.
	 * We'll just use the same one over and over whilst parsing our
	 * documents.
	 */

	if ((p = XML_ParserCreate(NULL)) == NULL)
		err(EXIT_FAILURE, "XML_ParserCreate");

	switch (op) {
	case OP_COMPILE:
		if (templ == NULL)
			templ = "article-template.xml";
		if (argc == 1) {
			rc = compile(p, templ, argv[0], outfile);
			break;
		}
		for (i = 0, rc = 1; rc && i < argc; i++)
			rc = compile(p, templ, argv[i], NULL);
		break;
	case OP_ATOM:
		if (fmtjson) {
			if (outfile == NULL)
				outfile = "blog.json";
			rc = json(p, argc, argv, outfile, asort);
			break;
		}
		if (templ == NULL)
			templ = "atom-template.xml";
		if (outfile == NULL)
			outfile = "atom.xml";
		rc = atom(p, templ, argc, argv, outfile, asort);
		break;
	case OP_LISTTAGS:
		rc = listtags(p, argc, argv,
			fmtjson, rev, fmtjson ? 0 : lf);
		break;
	case OP_LINK_INPLACE:
		if (templ == NULL)
			templ = "blog-template.xml";
		rc = linkall_r(p, templ, argc, argv, asort);
		break;
	default:
		if (templ == NULL)
			templ = "blog-template.xml";
		if (outfile == NULL)
			outfile = "blog.html";
		rc = linkall(p, templ, force, 
			argc, argv, outfile, asort);
		break;
	}

	sblg_destroy();
	XML_ParserFree(p);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s [-o file] [-t templ] -c file...\n"
		"       %s [-o file] [-t templ] [-s sort] -a file...\n"
		"       %s [-jlr] -l file...\n"
		"       %s [-t templ] [-s sort] -L file...\n"
		"       %s [-o file] [-s sort] -j file...\n"
		"       %s [-o file] [-t templ] [-s sort] -C file...\n"
		"       %s [-o file] [-t templ] [-s sort] file...\n",
		getprogname(), getprogname(), getprogname(), 
		getprogname(), getprogname(), getprogname(), 
		getprogname());
	return EXIT_FAILURE;
}
