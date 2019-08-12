This is a template for a front-page blog page linking to individual
articles.  As many articles appear as are listed.  (These can be limited
or an infinite scroll added.) It uses
[make](https://man.openbsd.org/make.1) to generate HTML5 articles from
XML sources.  The rendered output is visible at
https://kristaps.bsd.lv/sblg/examples/simple-frontpage.

It has the following features:

- super simple layout (mobile friendly)
- lightweight (only a CSS file)
- ability for articles to have a picture in either landscape or portrait
  mode
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
