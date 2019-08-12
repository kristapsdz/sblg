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
- [OpenGraph](https://ogp.me/) hooks (need to be customised, see `meta`
  elements in template)
- [Microdata](https://schema.org) hooks (need to be customised, see
  `itemprop` attributes in template)

Articles must be appended to the `ARTICLES` variable in the *Makefile*
in the order that they'll appear, last being the newest.

To directly publish, modify the `PREFIX` variable to a web server root
and run `make install`.  Or install into a staging directory and copy
from there.

The [OpenGraph](https://ogp.me/) and [Microdata](https://schema.org)
components should be properly filled in for sharing on social media and
indexing by search engines.

It requires the newest version of [sblg](https://kristaps.bsd.lv/sblg).

