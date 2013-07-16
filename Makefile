.SUFFIXES: .xml .html .1.html .1

PREFIX = /usr/local
CFLAGS += -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings 
OBJS = main.o compile.o linkall.o grok.o echo.o util.o atom.o
SRCS = main.c compile.c linkall.c grok.c echo.c util.c atom.c
ARTICLES = article1.html article2.html article3.html article4.html
XMLS = article1.xml article2.xml article3.xml
ATOM = atom.xml
XMLGENS = article-template.xml blog-template.xml
HTMLS = $(ARTICLES) blog.html sblg.1.html
CSSS = article.css blog.css shared.css
BINDIR = $(PREFIX)/bin
WWWDIR = /usr/vhosts/kristaps.bsd.lv/www/htdocs/sblg
MANDIR = $(PREFIX)/man
DOTAR = Makefile $(XMLS) $(CSSS) $(SRCS) blog-template.in.xml article-template.in.xml
VERSION = 0.0.7
VDATE = 2013-07-08

sblg: $(OBJS)
	$(CC) -o $@ $(OBJS) -lexpat

www: $(HTMLS) $(ATOM) sblg.tar.gz

installwww: www
	mkdir -p $(WWWDIR)
	install -m 0444 sblg.tar.gz Makefile $(ATOM) $(HTMLS) $(XMLS) $(XMLGENS) $(CSSS) $(WWWDIR)
	install -m 0444 sblg.tar.gz $(WWWDIR)/sblg-$(VERSION).tar.gz

install:
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	install -m 0755 sblg $(DESTDIR)$(BINDIR)
	install -m 0444 sblg.1 $(DESTDIR)$(MANDIR)/man1

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

main.o compile.o linkall.o grok.o echo.o: extern.h

atom.xml blog.html $(ARTICLES): sblg

atom.xml: atom-template.xml

blog.html: blog-template.xml

blog-template.xml: blog-template.in.xml
	sed -e "s!@VERSION@!$(VERSION)!g" \
	    -e "s!@VDATE@!$(VDATE)!g" blog-template.in.xml >$@

article-template.xml: article-template.in.xml
	sed -e "s!@VERSION@!$(VERSION)!g" \
	    -e "s!@VDATE@!$(VDATE)!g" article-template.in.xml >$@

$(ARTICLES): article-template.xml

blog.html: $(ARTICLES)
	./sblg -o $@ $(ARTICLES)

atom.xml: $(ARTICLES)
	./sblg -o $@ -a $(ARTICLES)

.xml.html:
	./sblg -o $@ -c $<

.1.1.html:
	mandoc -Thtml $< >$@

clean:
	rm -f sblg $(ATOM) $(OBJS) $(HTMLS) sblg.tar.gz $(XMLGENS)
	rm -rf *.dSYM
