/*	$Id$ */
/*
 * Copyright (c) 2013--2016 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifdef HAVE_PLEDGE
# include <unistd.h> /* pledge */
#endif

#include "extern.h"

enum	op {
	OP_ATOM,
	OP_COMPILE,
	OP_BLOG,
	OP_LISTTAGS,
	OP_LINK_INPLACE
};

#if HAVE_SANDBOX_INIT
static void
sandbox_apple(void)
{
	char	*ep;
	int	 rc;

	rc = sandbox_init(kSBXProfileNoNetwork, SANDBOX_NAMED, &ep);
	if (0 == rc)
		return;
	perror(ep);
	sandbox_free_error(ep);
	exit(EXIT_FAILURE);
}
#endif

#if HAVE_PLEDGE
static void
sandbox_openbsd(void)
{

	if (-1 == pledge("stdio cpath rpath wpath", NULL)) {
		perror("pledge");
		exit(EXIT_FAILURE);
	}
}
#endif

int
main(int argc, char *argv[])
{
	int		 ch, i, rc, fmtjson = 0;
	const char	*progname, *templ, *outfile, *force;
	enum op		 op;
	enum asort	 asort;
	XML_Parser	 p;

	setlocale(LC_ALL, "");

#if HAVE_SANDBOX_INIT
	sandbox_apple();
#elif HAVE_PLEDGE
	sandbox_openbsd();
#endif
	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	templ = outfile = force = NULL;
	op = OP_BLOG;
	asort = ASORT_DATE;

	while (-1 != (ch = getopt(argc, argv, "acjlLC:o:s:t:")))
		switch (ch) {
		case ('a'):
			op = OP_ATOM;
			break;
		case ('c'):
			op = OP_COMPILE;
			break;
		case ('C'):
			force = optarg;
			break;
		case ('j'):
			fmtjson = 1;
			break;
		case ('l'):
			op = OP_LISTTAGS;
			break;
		case ('L'):
			op = OP_LINK_INPLACE;
			break;
		case ('o'):
			outfile = optarg;
			break;
		case ('s'):
			if (0 == strcasecmp(optarg, "date"))
				asort = ASORT_DATE;
			else if (0 == strcasecmp(optarg, "filename"))
				asort = ASORT_FILENAME;
			else if (0 == strcasecmp(optarg, "cmdline"))
				asort = ASORT_CMDLINE;
			else
				goto usage;
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

	if (OP_BLOG == op && fmtjson)
		op = OP_ATOM;

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
		if (fmtjson) {
			if (NULL == outfile)
				outfile = "blog.json";
			rc = json(p, argc, argv, outfile, asort);
			break;
		}
		/*
		 * Merge multiple input files into an Atom feed
		 * amalgamation.
		 */
		if (NULL == templ)
			templ = "atom-template.xml";
		if (NULL == outfile)
			outfile = "atom.xml";
		rc = atom(p, templ, argc, 
			argv, outfile, asort);
		break;
	case (OP_LISTTAGS):
		/*
		 * List all tags and the filename(s) they're found in.
		 */
		rc = listtags(p, argc, argv, fmtjson, 0);
		break;
	case (OP_LINK_INPLACE):
		/*
		 * Merge multiple input files into multiple output files
		 * in a sort of multi-mode blog output.
		 */
		if (NULL == templ)
			templ = "blog-template.xml";
		rc = linkall_r(p, templ, argc, argv, asort);
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
		rc = linkall(p, templ, force, 
			argc, argv, outfile, asort);
		break;
	}

	XML_ParserFree(p);
	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);
usage:
	fprintf(stderr, 
		"usage: %s [-o file] [-t templ] -c file...\n"
		"       %s [-o file] [-t templ] [-s sort] -a file...\n"
		"       %s -l file...\n"
		"       %s [-t templ] [-s sort] -L file...\n"
		"       %s [-o file] [-s sort] -j file...\n"
		"       %s [-o file] [-t templ] [-s sort] -C file...\n"
		"       %s [-o file] [-t templ] [-s sort] file...\n",
		progname, progname, progname, 
		progname, progname, progname, progname);
	return(EXIT_FAILURE);
}
