This is a template for a front-page blog page linking to individual
Markdown articles.  It's in a horrific "brutalist" style that just
became my new favourite.  As many articles appear as are listed.  (These
can be limited or an infinite scroll added.) It uses
[make](https://man.openbsd.org/make.1) to generate HTML5 articles from
XML sources and [lowdown](https://kristaps.bsd.lv/lowdown) to convert
from Markdown.  The rendered output is visible at
https://kristaps.bsd.lv/sblg/examples/brutalist.

It has the following features:

- super simple layout (mobile friendly)
- lightweight (only a CSS file)
- rudimentary article tags (don't really do anything)
- Atom feed

It requires the newest version of [sblg](https://kristaps.bsd.lv/sblg).
To use:

1. Append articles to the `ARTICLES` variable in the
	[Makefile](Makefile) in the order that they'll appear, last
	being the newest.
2. Edit [template.xml](template.xml) with the proper name of your blog
	(the example is "Brutalist Template") and proper license.
3. Edit [index.xml](index.xml) with the proper name and description of
	your blog (the example is "Brutalist Template") and proper
	license.
4. Edit the URLs in [atom-template.xml](atom-template.xml).
5. To directly publish, modify the [Makefile](Makefile)'s `PREFIX`
	variable to a web server root and run `make install`.  Or
	install into a staging directory and copy from there.
