.SUFFIXES: .xml .html .1.html .1 .md .dot .svg

include Makefile.configure

VERSION 	 = 0.5.2
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
EXAMPLEDIR	 = $(DATADIR)/examples
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/sblg
DOTAR 		 = Makefile \
		   $(SRCS) \
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

www: $(HTMLS) $(BUILT) $(ATOM) sblg.tar.gz sblg.tar.gz.sha512 sblg
	( cd examples/simple && make SBLG=../../sblg )
	( cd examples/simple-frontpage && make SBLG=../../sblg )
	( cd examples/retro && make SBLG=../../sblg )

sblg.1: sblg.in.1
	sed "s!@SHAREDIR@!$(DATADIR)!g" sblg.in.1 >$@

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 Makefile $(BUILT) $(ATOM) $(HTMLS) $(XMLS) $(XMLGENS) $(MDS) $(CSSS) template*.jpg $(WWWDIR)
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz.sha512
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots
	( cd examples/simple && make install SBLG=../../sblg PREFIX=$(WWWDIR)/examples/simple )
	( cd examples/simple-frontpage && make install SBLG=../../sblg PREFIX=$(WWWDIR)/examples/simple-frontpage )
	( cd examples/retro && make install SBLG=../../sblg PREFIX=$(WWWDIR)/examples/retro )

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATADIR)
	mkdir -p $(DESTDIR)$(EXAMPLEDIR)/simple
	mkdir -p $(DESTDIR)$(EXAMPLEDIR)/simple-frontpage
	mkdir -p $(DESTDIR)$(EXAMPLEDIR)/retro
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_PROGRAM) sblg $(DESTDIR)$(BINDIR)
	$(INSTALL_MAN) sblg.1 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_DATA) schema.json $(DESTDIR)$(DATADIR)
	$(INSTALL_DATA) examples/simple/Makefile examples/simple/*.{xml,css,jpg,md} $(DESTDIR)$(EXAMPLEDIR)/simple
	$(INSTALL_DATA) examples/simple-frontpage/Makefile examples/simple-frontpage/*.{xml,css,jpg,md} $(DESTDIR)$(EXAMPLEDIR)/simple-frontpage
	$(INSTALL_DATA) examples/retro/Makefile examples/retro/atom-template.xml examples/retro/{template,index}.xml examples/retro/*.{css,md} $(DESTDIR)$(EXAMPLEDIR)/retro

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/sblg
	rm -f $(DESTDIR)$(MANDIR)/man1/sblg.1
	rm -f $(DESTDIR)$(DATADIR)/schema.json
	rm -f $(DESTDIR)$(DATADIR)/examples/*/Makefile
	rm -f $(DESTDIR)$(DATADIR)/examples/*/*.{xml,css,jpg,md}
	rmdir $(DESTDIR)$(DATADIR)/examples/simple
	rmdir $(DESTDIR)$(DATADIR)/examples/simple-frontpage
	rmdir $(DESTDIR)$(DATADIR)/examples/retro
	rmdir $(DESTDIR)$(DATADIR)/examples
	rmdir $(DESTDIR)$(DATADIR)

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	install -m 0755 configure .dist/sblg-$(VERSION)
	mkdir -p .dist/sblg-$(VERSION)/examples/retro
	mkdir -p .dist/sblg-$(VERSION)/examples/simple
	mkdir -p .dist/sblg-$(VERSION)/examples/simple-frontpage
	install -m 0644 examples/simple/Makefile examples/simple/*.{xml,css,jpg,md} .dist/sblg-$(VERSION)/examples/simple
	install -m 0644 examples/simple-frontpage/Makefile examples/simple-frontpage/*.{xml,css,jpg,md} .dist/sblg-$(VERSION)/examples/simple-frontpage
	install -m 0644 examples/retro/Makefile examples/retro/atom-template.xml examples/retro/{template,index}.xml examples/retro/*.{css,md} .dist/sblg-$(VERSION)/examples/retro
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
	rm -f examples/*/*.html examples/*/atom.xml
	rm -f examples/retro/article*.xml

distclean: clean
	rm -f Makefile.configure config.h config.log
