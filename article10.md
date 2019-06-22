date: 2016-12-07
title: Using Markdown

# Using Markdown

You can also use
[markdown](https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet)
for your articles by using
[lowdown](https://github.com/kristapsdz/lowdown)
(or really any translator).

[Markdown](https://github.com/adam-p/markdown-here/wiki/Markdown-Cheatsheet)
support isn't internal to [sblg(1)](https://kristaps.bsd.lv/sblg).
However, you can easily use it by translating the Markdown text into
XHTML and wrapping it with the appropriate `<article
data-sblg-article="1">` bits.

```
.md.xml:
	( echo "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" ; \
	  echo "<article data-sblg-article=\"1\">" ; \
	  lowdown $< ; \
	  echo "</article>" ; ) >$@

```

I use [lowdown](https://github.com/kristapsdz/lowdown) because I wrote
it (modernised fork of [hoedown](https://github.com/hoedown/hoedown))
and because it has some additional support for
[sblg(1)](https://kristaps.bsd.lv/sblg) like annotating the first
paragraph with an `<aside>`.

You can see this file in its original form as [article10.md](article10.md).
Enjoy!
