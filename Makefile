.SUFFIXES: .xml .html .1.html .1

VERSION 	 = 0.2.4
VDATE 		 = 2014-12-08
PREFIX 		 = /usr/local
CFLAGS 		+= -g -W -Wall -Wstrict-prototypes -Wno-unused-parameter -Wwrite-strings 
OBJS		 = main.o \
		   compat-reallocarray.o \
		   compat-strlcat.o \
		   compat-strlcpy.o \
		   compile.o \
		   linkall.o \
		   grok.o \
		   echo.o \
		   util.o \
		   atom.o \
		   article.o
SRCS		 = main.c \
		   compile.c \
		   compat-reallocarray.c \
		   compat-strlcat.c \
		   compat-strlcpy.c \
		   linkall.c \
		   grok.c \
		   echo.c \
		   util.c \
		   atom.c \
		   article.c
TESTS 		 = test-reallocarray.c \
      		   test-strlcat.c \
      		   test-strlcpy.c 
ARTICLES 	 = article1.html \
	 	   article2.html \
	 	   article4.html \
	 	   article5.html \
	 	   article6.html \
	 	   article7.html \
	 	   article8.html
VERSIONS	 = version_0_0_13.xml \
		   version_0_1_1.xml \
		   version_0_1_2.xml \
		   version_0_1_3.xml \
		   version_0_2_1.xml \
		   version_0_2_2.xml \
		   version_0_2_3.xml \
		   version_0_2_4.xml 
XMLS		 = article1.xml \
    		   article2.xml \
    		   article4.xml \
    		   article5.xml \
    		   article6.xml \
    		   $(VERSIONS)
ATOM 		 = atom.xml
XMLGENS 	 = article.xml index.xml
HTMLS 		 = $(ARTICLES) index.html sblg.1.html
CSSS 		 = article.css index.css 
BINDIR 		 = $(PREFIX)/bin
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/sblg
MANDIR 		 = $(PREFIX)/man
DOTAR 		 = Makefile \
		   $(XMLS) \
		   $(CSSS) \
		   $(SRCS) \
		   $(XMLGENS) \
		   atom-template.xml \
		   sblg.1 \
		   extern.h \
		   configure \
		   config.h.post \
		   config.h.pre \
		   $(TESTS)

sblg: $(OBJS)
	$(CC) -o $@ $(OBJS) -lexpat

www: $(HTMLS) $(ATOM) sblg.tar.gz sblg.tar.gz.sha512

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 Makefile $(ATOM) $(HTMLS) $(XMLS) $(XMLGENS) $(CSSS) $(WWWDIR)
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz.sha512
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots

install: sblg
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	install -m 0755 sblg $(DESTDIR)$(BINDIR)
	install -m 0444 sblg.1 $(DESTDIR)$(MANDIR)/man1

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

sblg.tar.gz.sha512: sblg.tar.gz
	openssl dgst -sha512 sblg.tar.gz >$@

config.h: config.h.pre config.h.post configure $(TESTS)
	rm -f config.log
	CC="$(CC)" CFLAGS="$(CFLAGS)" ./configure

$(OBJS): extern.h config.h

atom.xml index.html $(ARTICLES): sblg

atom.xml: atom-template.xml

$(ARTICLES): article.xml

index.html: index.xml $(ARTICLES) $(VERSIONS)
	./sblg -o- -t index.xml $(ARTICLES) $(VERSIONS) | \
		sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@VDATE@!$(VDATE)!g" >$@

atom.xml: $(ARTICLES) $(VERSIONS)
	./sblg -o $@ -a $(ARTICLES) $(VERSIONS)

.xml.html:
	./sblg -o- -t article.xml -c $< | \
		sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@VDATE@!$(VDATE)!g" >$@

.1.1.html:
	mandoc -Thtml $< >$@

clean:
	rm -f sblg $(ATOM) $(OBJS) $(HTMLS) sblg.tar.gz sblg.tar.gz.sha512
	rm -f config.h config.log
	rm -rf *.dSYM
