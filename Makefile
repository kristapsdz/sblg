.SUFFIXES: .xml .html .1.html .1 .md

include Makefile.configure

VERSION 	 = 0.4.10
VDATE 		 = 2017-10-29
CFLAGS		+= -DVERSION=\"$(VERSION)\"
OBJS		 = compats.o \
		   main.o \
		   compile.o \
		   linkall.o \
		   grok.o \
		   util.o \
		   atom.o \
		   article.o \
		   json.o \
		   listtags.o
SRCS		 = compats.c \
		   main.c \
		   compile.c \
		   linkall.c \
		   grok.c \
		   util.c \
		   atom.c \
		   article.c \
		   json.c \
		   listtags.c \
		   tests.c
ARTICLES 	 = article1.html \
	 	   article2.html \
	 	   article4.html \
	 	   article5.html \
	 	   article6.html \
	 	   article7.html \
	 	   article8.html \
	 	   article9.html \
	 	   article10.html 
ARTICLEXMLS 	 = article1.xml \
	 	   article2.xml \
	 	   article4.xml \
	 	   article5.xml \
	 	   article6.xml \
	 	   article7.xml \
	 	   article8.xml \
	 	   article9.xml \
		   article10.xml
XMLS		 = $(ARTICLEXMLS) \
		   versions.xml
ATOM 		 = atom.xml
XMLGENS 	 = article.xml index.xml
HTMLS 		 = $(ARTICLES) index.html archive.html sblg.1.html
CSSS 		 = article.css index.css mandoc.css
MDS		 = article10.md
DATADIR	 	 = $(SHAREDIR)/sblg
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/sblg
DOTAR 		 = Makefile \
		   $(XMLS) \
		   $(CSSS) \
		   $(SRCS) \
		   $(XMLGENS) \
		   atom-template.xml \
		   sblg.in.1 \
		   sblg.h \
		   schema.json \
		   extern.h

all: sblg sblg.a sblg.1

sblg: $(OBJS)
	$(CC) -o $@ $(OBJS) -lexpat

sblg.a: $(OBJS)
	$(AR) rs $@ $(OBJS)

www: $(HTMLS) $(ATOM) sblg.tar.gz sblg.tar.gz.sha512

sblg.1: sblg.in.1
	sed "s!@SHAREDIR@!$(DATADIR)!g" sblg.in.1 >$@

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 Makefile $(ATOM) $(HTMLS) $(XMLS) $(XMLGENS) $(MDS) $(CSSS) $(WWWDIR)
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz.sha512
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	install -m 0755 sblg $(DESTDIR)$(BINDIR)
	install -m 0444 sblg.1 $(DESTDIR)$(MANDIR)/man1
	install -m 0444 schema.json $(DESTDIR)$(DATADIR)

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	install -m 0755 configure .dist/sblg-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

sblg.tar.gz.sha512: sblg.tar.gz
	sha512 sblg.tar.gz >$@

$(OBJS): sblg.h extern.h config.h

atom.xml index.html $(ARTICLES): sblg

atom.xml: atom-template.xml

$(ARTICLES): article.xml

index.html: index.xml $(ARTICLES) versions.xml
	./sblg -o- -t index.xml $(ARTICLES) versions.xml | \
		sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@VDATE@!$(VDATE)!g" >$@

archive.html: archive.xml $(ARTICLES) versions.xml
	./sblg -o- -t archive.xml $(ARTICLES) versions.xml | \
		sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@VDATE@!$(VDATE)!g" >$@

atom.xml: $(ARTICLES) versions.xml
	./sblg -o $@ -a $(ARTICLES) versions.xml

.xml.html:
	./sblg -o- -t article.xml -c $< | \
		sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@VDATE@!$(VDATE)!g" >$@

article9.html: $(ARTICLEXMLS)
	./sblg -o- -t article.xml -C article9.xml $(ARTICLEXMLS) | \
		sed -e "s!@VERSION@!$(VERSION)!g" -e "s!@VDATE@!$(VDATE)!g" >$@

.1.1.html:
	mandoc -Ostyle=mandoc.css -Thtml $< >$@

.md.xml:
	( echo "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" ; \
	  echo "<article data-sblg-article=\"1\" data-sblg-tags=\"howto\">" ; \
	  lowdown $< ; \
	  echo "</article>" ; ) >$@

clean:
	rm -f sblg $(ATOM) $(OBJS) $(HTMLS) sblg.tar.gz sblg.tar.gz.sha512 sblg.1
	rm -f article10.xml
	rm -rf *.dSYM

distclean: clean
	rm -f Makefile.configure config.h config.log
