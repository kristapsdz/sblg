.SUFFIXES: .xml .html .1.html .1

PREFIX = /usr/local
CFLAGS += -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings 
OBJS = main.o compile.o linkall.o grok.o util.o
SRCS = main.c compile.c linkall.c grok.c util.c
XMLS = article1.xml article2.xml article3.xml article-template.xml 
XMLGENS = blog-template.xml
HTMLS = article1.html article2.html article3.html blog.html sblg.1.html
CSSS = article.css blog.css shared.css
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man
DOTAR = Makefile $(XMLS) $(CSSS) $(SRCS) blog-template.in.xml
VERSION = 0.0.5
VDATE = 2013-07-03

sblg: $(OBJS)
	$(CC) -o $@ $(OBJS) -lexpat

www: $(HTMLS) sblg.tar.gz

installwww: www
	mkdir -p $(PREFIX)
	install -m 0444 sblg.tar.gz Makefile $(HTMLS) $(XMLS) $(XMLGENS) $(CSSS) $(PREFIX)
	install -m 0444 sblg.tar.gz $(PREFIX)/sblg-$(VERSION).tar.gz

install:
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	install -m 0755 sblg $(DESTDIR)$(BINDIR)

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

main.o compile.o linkall.o grok.o: extern.h

blog.html article1.html article2.html article3.html: sblg

blog.html: blog-template.xml

blog-template.xml: blog-template.in.xml
	sed -e "s!@VERSION@!$(VERSION)!g" \
	    -e "s!@VDATE@!$(VDATE)!g" blog-template.in.xml >$@

article1.html article2.html article3.html: article-template.xml

blog.html: article1.html article2.html article3.html
	./sblg -o $@ article1.html article2.html article3.html

.xml.html:
	./sblg -c -o $@ $<

.1.1.html:
	mandoc -Thtml $< >$@

clean:
	rm -f sblg $(OBJS) $(HTMLS) sblg.tar.gz $(XMLGENS)
	rm -rf *.dSYM
