.SUFFIXES: .xml .html .1.html .1 .md .dot .svg

include Makefile.configure

VERSION 	 = 0.4.29
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
BUILT		 = index1.svg \
		   index2.svg \
		   index3.svg

all: sblg sblg.a sblg.1

sblg: $(OBJS)
	$(CC) -o $@ $(OBJS) -lexpat

sblg.a: $(OBJS)
	$(AR) rs $@ $(OBJS)

www: $(HTMLS) $(BUILT) $(ATOM) sblg.tar.gz sblg.tar.gz.sha512

sblg.1: sblg.in.1
	sed "s!@SHAREDIR@!$(DATADIR)!g" sblg.in.1 >$@

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 Makefile $(BUILT) $(ATOM) $(HTMLS) $(XMLS) $(XMLGENS) $(MDS) $(CSSS) $(WWWDIR)
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz.sha512
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(DATADIR)/examples
	mkdir -p $(DESTDIR)$(DATADIR)/examples/simple
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_PROGRAM) sblg $(DESTDIR)$(BINDIR)
	$(INSTALL_MAN) sblg.1 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_DATA) schema.json $(DESTDIR)$(DATADIR)
	$(INSTALL_DATA) examples/simple/Makefile examples/simple/*.{xml,css,jpg} $(DESTDIR)$(DATADIR)/examples/simple

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/sblg
	rm -f $(DESTDIR)$(MANDIR)/man1/sblg.1
	rm -f $(DESTDIR)$(DATADIR)/schema.json
	rmdir $(DESTDIR)$(DATADIR)

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	install -m 0755 configure .dist/sblg-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

sblg.tar.gz.sha512: sblg.tar.gz
	sha512 sblg.tar.gz >$@

$(OBJS): sblg.h extern.h config.h version.h

atom.xml index.html $(ARTICLES): sblg

atom.xml: atom-template.xml

$(ARTICLES): article.xml

version.h: Makefile
	echo "#define VERSION \"$(VERSION)\"" >$@

index.html: index.xml $(ARTICLES) versions.xml
	./sblg -o- -t index.xml $(ARTICLES) versions.xml >$@

archive.html: archive.xml $(ARTICLES) versions.xml
	./sblg -o- -t archive.xml $(ARTICLES) versions.xml >$@

atom.xml: $(ARTICLES) versions.xml
	./sblg -s date -o $@ -a $(ARTICLES) versions.xml

.xml.html:
	./sblg -o- -t article.xml -c $< >$@

$(ARTICLES): versions.xml
	./sblg -o- -t article.xml -C $< $< versions.xml >$@

.1.1.html:
	mandoc -Ostyle=mandoc.css -Thtml $< >$@

.dot.svg:
	dot -Tsvg $< | xsltproc --novalid notugly.xsl - >$@

.md.xml:
	( echo "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" ; \
	  echo "<article data-sblg-article=\"1\" data-sblg-tags=\"howto\" " ; \
	  echo " data-sblg-datetime=\"`lowdown -X date $<`\">"; \
	  lowdown $< ; \
	  echo "</article>" ; ) >$@

clean:
	rm -f sblg $(ATOM) $(OBJS) $(HTMLS) $(BUILT) sblg.tar.gz sblg.tar.gz.sha512 sblg.1
	rm -f article10.xml
	rm -f version.h

distclean: clean
	rm -f Makefile.configure config.h config.log
