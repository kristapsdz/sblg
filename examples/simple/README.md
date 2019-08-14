This is a simple one-page-per-article blog style, with the default page
being the newest.  It uses [make](https://man.openbsd.org/make.1) to
generate HTML5 articles from XML sources.  The rendered output is visible at
https://kristaps.bsd.lv/sblg/examples/simple.

It has the following features:

- super simple layout (mobile friendly)
- lightweight (only a CSS file)
- ability for an author to have a picture (with the attribute
  `data-sblg-set-profile` somewhere in the article)
- rudimentary article tags (don't really do anything)
- Atom feed

It requires the newest version of [sblg](https://kristaps.bsd.lv/sblg).
To use:

1. Append articles to the `ARTICLES` variable in the
	[Makefile](Makefile) in the order that they'll appear, last
	being the newest.
2. Edit [template.xml](template.xml) with the proper name of your blog
	(the example is "Simple Template Blog") and proper license.
3. Edit the URLs in [atom-template.xml](atom-template.xml).
4. To directly publish, modify the [Makefile](Makefile)'s `PREFIX`
	variable to a web server root and run `make install`.  Or
	install into a staging directory and copy from there.

