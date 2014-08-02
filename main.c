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
#include <expat.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "extern.h"

enum	op {
	OP_ATOM,
	OP_COMPILE,
	OP_BLOG
};

int
main(int argc, char *argv[])
{
	int		 ch, i, rc;
	const char	*progname, *templ, *outfile, *force;
	enum op		 op;
	XML_Parser	 p;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	templ = outfile = force = NULL;
	op = OP_BLOG;

	while (-1 != (ch = getopt(argc, argv, "acf:o:t:")))
		switch (ch) {
		case ('a'):
			op = OP_ATOM;
			break;
		case ('c'):
			op = OP_COMPILE;
			break;
		case ('f'):
			force = optarg;
			break;
		case ('o'):
			outfile = optarg;
			break;
		case ('t'):
			templ = optarg;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc)
		goto usage;

	/*
	 * Avoid constantly re-using a parser by specifying one here.
	 * We'll just use the same one over and over whilst parsing our
	 * documents.
	 */
	if (NULL == (p = XML_ParserCreate(NULL))) {
		perror(NULL);
		return(EXIT_FAILURE);
	}

	switch (op) {
	case (OP_COMPILE):
		/*
		 * Merge a single input XML file into a template XML
		 * files to produce output.
		 * (This can happen multiple times if we're spitting
		 * into stdout.)
		 */
		if (NULL == templ)
			templ = "article-template.xml";
		if (1 == argc) {
			rc = compile(p, templ, argv[0], outfile);
			break;
		}
		for (i = 0, rc = 1; rc && i < argc; i++)
			rc = compile(p, templ, argv[i], NULL);
		break;
	case (OP_ATOM):
		/*
		 * Merge multiple input files into an Atom feed
		 * amalgamation.
		 */
		if (NULL == templ)
			templ = "atom-template.xml";
		if (NULL == outfile)
			outfile = "atom.xml";
		rc = atom(p, templ, argc, argv, outfile);
		break;
	default:
		/*
		 * Merge multiple input files into a regular (we'll call
		 * it "blog") amalgamation.
		 */
		if (NULL == templ)
			templ = "blog-template.xml";
		if (NULL == outfile)
			outfile = "blog.html";
		rc = linkall(p, templ, force, argc, argv, outfile);
		break;
	}

	XML_ParserFree(p);
	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);
usage:
	fprintf(stderr, "usage: %s [-o file] [-t templ] -c file...\n"
			"       %s [-o file] [-t templ] -a file...\n"
			"       %s [-o file] [-t templ] file...\n",
			progname, progname, progname);
	return(EXIT_FAILURE);
}
