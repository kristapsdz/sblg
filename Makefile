.SUFFIXES: .xml .html .1.html .1 .md .dot .svg

include Makefile.configure

VERSION 	 = 0.5.4
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
XMLS		 = versions.xml
ATOM 		 = atom.xml
HTMLS 		 = archive.html \
		   example1.html \
		   example2.html \
		   example3.html \
		   index.html \
		   sblg.1.html
CSSS 		 = index.css
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
IMAGES		 = template1.jpg \
		   template2.jpg \
		   template3.jpg \
		   template4.jpg \
		   template5.jpg \
		   template6.jpg

all: sblg sblg.a sblg.1

sblg: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -lexpat

sblg.a: $(OBJS)
	$(AR) rs $@ $(OBJS)

www: $(HTMLS) $(BUILT) $(ATOM) sblg.tar.gz sblg.tar.gz.sha512 sblg
	( cd examples/simple && make SBLG=../../sblg )
	( cd examples/simple-frontpage && make SBLG=../../sblg )
	( cd examples/retro && make SBLG=../../sblg )
	( cd examples/brutalist && make SBLG=../../sblg )
	( cd examples/photos-column && make SBLG=../../sblg )
	( cd examples/photos-grid && make SBLG=../../sblg )

sblg.1: sblg.in.1
	sed "s!@SHAREDIR@!$(DATADIR)!g" sblg.in.1 >$@

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 Makefile $(BUILT) $(IMAGES) $(ATOM) $(HTMLS) $(CSSS) $(WWWDIR)
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz.sha512
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots
	( cd examples/simple && make installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/simple )
	( cd examples/simple-frontpage && make installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/simple-frontpage )
	( cd examples/retro && make installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/retro )
	( cd examples/brutalist && make installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/brutalist )
	( cd examples/photos-column && make installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/photos-column )
	( cd examples/photos-grid && make installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/photos-grid )

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
	( cd examples/simple && make install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/simple )
	( cd examples/simple-frontpage && make install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/simple-frontpage )
	( cd examples/retro && make install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/retro )
	( cd examples/brutalist && make install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/brutalist )
	( cd examples/photos-column && make install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/photos-column )
	( cd examples/photos-grid && make install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/photos-grid )

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	install -m 0755 configure .dist/sblg-$(VERSION)
	mkdir -p .dist/sblg-$(VERSION)/examples/retro
	mkdir -p .dist/sblg-$(VERSION)/examples/brutalist
	mkdir -p .dist/sblg-$(VERSION)/examples/photos-column
	mkdir -p .dist/sblg-$(VERSION)/examples/photos-grid
	mkdir -p .dist/sblg-$(VERSION)/examples/simple-frontpage
	( cd examples/simple && make install PREFIX=../../.dist/sblg-$(VERSION)/examples/simple )
	( cd examples/simple-frontpage && make install PREFIX=../../.dist/sblg-$(VERSION)/examples/simple-frontpage )
	( cd examples/retro && make install PREFIX=../../.dist/sblg-$(VERSION)/examples/retro )
	( cd examples/brutalist && make install PREFIX=../../.dist/sblg-$(VERSION)/examples/brutalist )
	( cd examples/photos-column && make install PREFIX=../../.dist/sblg-$(VERSION)/examples/photos-column )
	( cd examples/photos-grid && make install PREFIX=../../.dist/sblg-$(VERSION)/examples/photos-grid )
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

sblg.tar.gz.sha512: sblg.tar.gz
	sha512 sblg.tar.gz >$@

$(OBJS): sblg.h extern.h config.h version.h

atom.xml $(HTMLS): sblg

atom.xml: atom-template.xml

version.h: Makefile
	echo "#define VERSION \"$(VERSION)\"" >$@

index.html: index.xml versions.xml
	./sblg -o- -t index.xml versions.xml >$@

archive.html: archive.xml versions.xml
	./sblg -o- -t archive.xml versions.xml >$@

example1.html: example1-template.xml example1-article.xml
	./sblg -t example1-template.xml -o $@ -c example1-article.xml

example2.html: example2-template.xml example2-article1.xml example2-article2.xml
	./sblg -t example2-template.xml -o $@ example2-article1.xml example2-article2.xml

example3.html: example3-template.xml example2-article1.xml example2-article2.xml
	./sblg -t example3-template.xml -o $@ -C example2-article1.xml example2-article1.xml example2-article2.xml

atom.xml: versions.xml
	./sblg -s date -o $@ -a versions.xml

.1.1.html:
	mandoc -Ostyle=https://bsd.lv/css/mandoc.css -Thtml $< >$@

.dot.svg:
	dot -Tsvg $< | xsltproc --novalid notugly.xsl - >$@

clean:
	rm -f sblg $(ATOM) $(OBJS) $(HTMLS) $(BUILT) sblg.tar.gz sblg.tar.gz.sha512 sblg.1
	rm -f version.h
	( cd examples/simple && make clean )
	( cd examples/simple-frontpage && make clean )
	( cd examples/retro && make clean )
	( cd examples/brutalist && make clean )
	( cd examples/photos-column && make clean )
	( cd examples/photos-grid && make clean )

distclean: clean
	rm -f Makefile.configure config.h config.log
