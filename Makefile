.PHONY: regress regress_rebuild
.SUFFIXES: .xml .html .1.html .1 .md .dot .svg

include Makefile.configure

WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/sblg

sinclude Makefile.local

VERSION 	 = 0.6.1
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
CSSS 		 = article.css \
		   index.css
DATADIR	 	 = $(SHAREDIR)/sblg
EXAMPLEDIR	 = $(DATADIR)/examples
DOTAR 		 = Makefile \
		   $(SRCS) \
		   sblg.in.1 \
		   sblg.h \
		   schema.json \
		   extern.h \
		   article.css \
		   article.xml \
		   article1.xml \
		   article2.xml \
		   article4.xml \
		   article5.xml \
		   article6.xml \
		   article7.xml \
		   article8.xml \
		   article9.xml \
		   article10.md
BUILT		 = article10.xml \
		   index1.svg \
		   index2.svg \
		   index3.svg \
		   index4.svg
IMAGES		 = template1.jpg \
		   template2.jpg \
		   template3.jpg \
		   template4.jpg \
		   template5.jpg \
		   template6.jpg
ARTICLES	 = article1.html \
		   article2.html \
		   article4.html \
		   article5.html \
		   article6.html \
		   article7.html \
		   article8.html \
		   article9.html \
		   article10.html

LDADD_PKG	!= pkg-config --libs expat 2>/dev/null || echo "-lexpat"
CFLAGS_PKG 	!= pkg-config --cflags expat 2>/dev/null || echo ""
LDADD		+= $(LDADD_PKG)
CFLAGS		+= $(CFLAGS_PKG)
# If this command not found, the JSON test is skipped.
JQ		 = jq
VALGRIND	 = valgrind
VALGRIND_ARGS	 = -q --leak-check=full --leak-resolution=high --show-reachable=yes
REGRESS_ENV	 = TZ=GMT LC_ALL=en_US

all: sblg sblg.a sblg.1

sblg: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LDADD)

sblg.a: $(OBJS)
	$(AR) rs $@ $(OBJS)

www: $(HTMLS) $(ARTICLES) $(BUILT) $(ATOM) sblg.tar.gz sblg.tar.gz.sha512 sblg
	( cd examples/simple && $(MAKE) SBLG=../../sblg )
	( cd examples/simple-frontpage && $(MAKE) SBLG=../../sblg )
	( cd examples/retro && $(MAKE) SBLG=../../sblg )
	( cd examples/brutalist && $(MAKE) SBLG=../../sblg )
	( cd examples/photos-column && $(MAKE) SBLG=../../sblg )
	( cd examples/photos-grid && $(MAKE) SBLG=../../sblg )

sblg.1: sblg.in.1
	sed "s!@SHAREDIR@!$(DATADIR)!g" sblg.in.1 >$@

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 Makefile $(BUILT) $(IMAGES) $(ATOM) $(HTMLS) $(CSSS) $(ARTICLES) $(WWWDIR)
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots/sblg-$(VERSION).tar.gz.sha512
	install -m 0444 sblg.tar.gz $(WWWDIR)/snapshots
	install -m 0444 sblg.tar.gz.sha512 $(WWWDIR)/snapshots
	( cd examples/simple && $(MAKE) installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/simple )
	( cd examples/simple-frontpage && $(MAKE) installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/simple-frontpage )
	( cd examples/retro && $(MAKE) installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/retro )
	( cd examples/brutalist && $(MAKE) installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/brutalist )
	( cd examples/photos-column && $(MAKE) installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/photos-column )
	( cd examples/photos-grid && $(MAKE) installwww SBLG=../../sblg WWWDIR=$(WWWDIR)/examples/photos-grid )

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
	( cd examples/simple && $(MAKE) install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/simple )
	( cd examples/simple-frontpage && $(MAKE) install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/simple-frontpage )
	( cd examples/retro && $(MAKE) install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/retro )
	( cd examples/brutalist && $(MAKE) install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/brutalist )
	( cd examples/photos-column && $(MAKE) install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/photos-column )
	( cd examples/photos-grid && $(MAKE) install PREFIX=$(DESTDIR)$(EXAMPLEDIR)/photos-grid )

sblg.tar.gz:
	mkdir -p .dist/sblg-$(VERSION)/
	install -m 0644 $(DOTAR) .dist/sblg-$(VERSION)
	install -m 0755 configure .dist/sblg-$(VERSION)
	mkdir -p .dist/sblg-$(VERSION)/examples/retro
	mkdir -p .dist/sblg-$(VERSION)/examples/brutalist
	mkdir -p .dist/sblg-$(VERSION)/examples/photos-column
	mkdir -p .dist/sblg-$(VERSION)/examples/photos-grid
	mkdir -p .dist/sblg-$(VERSION)/examples/simple-frontpage
	( cd examples/simple && $(MAKE) install PREFIX=../../.dist/sblg-$(VERSION)/examples/simple )
	( cd examples/simple-frontpage && $(MAKE) install PREFIX=../../.dist/sblg-$(VERSION)/examples/simple-frontpage )
	( cd examples/retro && $(MAKE) install PREFIX=../../.dist/sblg-$(VERSION)/examples/retro )
	( cd examples/brutalist && $(MAKE) install PREFIX=../../.dist/sblg-$(VERSION)/examples/brutalist )
	( cd examples/photos-column && $(MAKE) install PREFIX=../../.dist/sblg-$(VERSION)/examples/photos-column )
	( cd examples/photos-grid && $(MAKE) install PREFIX=../../.dist/sblg-$(VERSION)/examples/photos-grid )
	mkdir -p .dist/sblg-$(VERSION)/regress
	mkdir -p .dist/sblg-$(VERSION)/regress/standalone
	mkdir -p .dist/sblg-$(VERSION)/regress/blog
	mkdir -p .dist/sblg-$(VERSION)/regress/json
	install -m 0644 regress/standalone/*.html regress/standalone/*.xml .dist/sblg-$(VERSION)/regress/standalone
	install -m 0644 regress/blog/*.html regress/blog/*.xml .dist/sblg-$(VERSION)/regress/blog
	install -m 0644 regress/json/*.xml regress/json/*.json .dist/sblg-$(VERSION)/regress/json
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

sblg.tar.gz.sha512: sblg.tar.gz
	openssl dgst -sha512 -hex sblg.tar.gz >$@

distcheck: sblg.tar.gz.sha512
	mandoc -Tlint -Werror sblg.in.1
	newest=`grep "<h1>" versions.xml | tail -1 | sed 's![ 	]*!!g'` ; \
	       [ "$$newest" = "<h1>$(VERSION)</h1>" ] || \
		{ echo "Version $(VERSION) not newest in versions.xml" 1>&2 ; exit 1 ; }
	rm -rf .distcheck
	[ "`openssl dgst -sha512 -hex sblg.tar.gz`" = "`cat sblg.tar.gz.sha512`" ] || \
 		{ echo "Checksum does not match." 1>&2 ; exit 1 ; }
	mkdir -p .distcheck
	( cd .distcheck && tar -zvxpf ../sblg.tar.gz )
	( cd .distcheck/sblg-$(VERSION) && ./configure PREFIX=prefix )
	( cd .distcheck/sblg-$(VERSION) && $(MAKE) )
	( cd .distcheck/sblg-$(VERSION) && $(MAKE) regress )
	( cd .distcheck/sblg-$(VERSION) && $(MAKE) install )
	rm -rf .distcheck

$(OBJS): sblg.h extern.h config.h version.h

$(ARTICLES): article.xml

atom.xml $(HTMLS) $(ARTICLES): sblg

atom.xml: atom-template.xml

version.h: Makefile
	echo "#define VERSION \"$(VERSION)\"" >$@

index.html: index.xml versions.xml $(ARTICLES)
	./sblg -o- -t index.xml versions.xml coverage.xml $(ARTICLES) >$@

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

.xml.html:
	./sblg -t article.xml -o $@ -c $<

.dot.svg:
	dot -Tsvg $< | xsltproc --novalid notugly.xsl - >$@

.md.xml:
	( echo "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" ; \
	  echo "<article data-sblg-article=\"1\" data-sblg-tags=\"howto\">" ; \
	  lowdown $< ; \
	  echo "</article>" ; ) >$@

clean:
	rm -f sblg $(ARTICLES) $(ATOM) $(OBJS) $(HTMLS) $(BUILT) sblg.tar.gz sblg.tar.gz.sha512 sblg.1
	rm -f *.gcno *.gcov *.gcda
	rm -f version.h
	( cd examples/simple && $(MAKE) clean )
	( cd examples/simple-frontpage && $(MAKE) clean )
	( cd examples/retro && $(MAKE) clean )
	( cd examples/brutalist && $(MAKE) clean )
	( cd examples/photos-column && $(MAKE) clean )
	( cd examples/photos-grid && $(MAKE) clean )

regress_rebuild: all
	@tmp=`mktemp` ; \
	for f in regress/standalone/*.in.xml ; do \
		d=`dirname $$f` ; \
		tf=`basename $$f .in.xml`.template.xml ; \
		[ -f $$d/$$tf ] || tf=simple.template.xml ; \
		vf=`basename $$f .in.xml`.html ; \
		${REGRESS_ENV} ./sblg -o- -t $$d/$$tf -c $$f >$$tmp 2>/dev/null ; \
		[ -f $$d/$$vf ] || { \
			echo "$$f... creating" ; \
			cp $$tmp $$d/$$vf ; \
			continue ; \
		} ; \
		diff $$tmp $$d/$$vf 2>/dev/null 1>&2 || { \
			echo "$$f... replacing" ; \
			set +e ; \
			diff -u $$d/$$vf $$tmp ; \
			set -e ; \
			cp $$tmp $$d/$$vf ; \
			continue ; \
		} ; \
		echo "$$f... same" ; \
	done ; \
	for f in regress/blog/*.in.xml ; do \
		d=`dirname $$f` ; \
		tf=`basename $$f .in.xml`.template.xml ; \
		[ -f $$d/$$tf ] || tf=simple.template.xml ; \
		vf=`basename $$f .in.xml`.html ; \
		${REGRESS_ENV} ./sblg -o- -t $$d/$$tf $$f >$$tmp 2>/dev/null ; \
		[ -f $$d/$$vf ] || { \
			echo "$$f... creating" ; \
			cp $$tmp $$d/$$vf ; \
			continue ; \
		} ; \
		diff $$tmp $$d/$$vf 2>/dev/null 1>&2 || { \
			echo "$$f... replacing" ; \
			set +e ; \
			diff -u $$d/$$vf $$tmp ; \
			set -e ; \
			cp $$tmp $$d/$$vf ; \
			continue ; \
		} ; \
		echo "$$f... same" ; \
	done ; \
	for tf in regress/blog/*.template.xml ; do \
		d=`dirname $$tf` ; \
		f=`basename $$tf .template.xml`.in.xml ; \
		[ ! -f $$d/$$f ] || continue ; \
		f=$$d/simple.in.xml ; \
		vf=`basename $$tf .template.xml`.html ; \
		TZ${REGRESS_ENV}GMT ./sblg -o- -t $$tf $$f >$$tmp 2>/dev/null ; \
		[ -f $$d/$$vf ] || { \
			echo "$$tf... creating" ; \
			cp $$tmp $$d/$$vf ; \
			continue ; \
		} ; \
		diff $$tmp $$d/$$vf 2>/dev/null 1>&2 || { \
			echo "$$tf... replacing" ; \
			set +e ; \
			diff -u $$d/$$vf $$tmp ; \
			set -e ; \
			cp $$tmp $$d/$$vf ; \
			continue ; \
		} ; \
		echo "$$tf... same" ; \
	done ; \
	rm -f $$tmp

valgrind: all
	@tmp=`mktemp` ; \
	for f in regress/standalone/*.in.xml ; do \
		d=`dirname $$f` ; \
		tf=`basename $$f .in.xml`.template.xml ; \
		[ -f $$d/$$tf ] || tf=simple.template.xml ; \
		vf=`basename $$f .in.xml`.html ; \
		$(VALGRIND) $(VALGRIND_ARGS) --log-file=$$tmp \
			./sblg -o- -t $$d/$$tf -c $$f 2>&1 >/dev/null ; \
		[ ! -s $$tmp ] || { \
			echo "$$f... fail" ; \
			cat $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		} ; \
		echo "$$f... ok" ; \
	done ; \
	for f in regress/blog/*.in.xml ; do \
		d=`dirname $$f` ; \
		tf=`basename $$f .in.xml`.template.xml ; \
		[ -f $$d/$$tf ] || tf=simple.template.xml ; \
		vf=`basename $$f .in.xml`.html ; \
		$(VALGRIND) $(VALGRIND_ARGS) --log-file=$$tmp \
			./sblg -o- -t $$d/$$tf $$f 2>&1 >/dev/null ; \
		[ ! -s $$tmp ] || { \
			echo "$$f... fail" ; \
			cat $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		} ; \
		echo "$$f... ok" ; \
	done ; \
	for tf in regress/blog/*.template.xml ; do \
		d=`dirname $$tf` ; \
		f=`basename $$tf .template.xml`.in.xml ; \
		[ ! -f $$d/$$f ] || continue ; \
		f=$$d/simple.in.xml ; \
		vf=`basename $$tf .template.xml`.html ; \
		$(VALGRIND) $(VALGRIND_ARGS) --log-file=$$tmp \
	       		./sblg -o- -t $$tf $$f >$$tmp 2>&1 >/dev/null ; \
		[ ! -s $$tmp ] || { \
			echo "$$tf... fail" ; \
			cat $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		} ; \
		echo "$$tf... ok" ; \
	done ; \
	$(VALGRIND) $(VALGRIND_ARGS) --log-file=$$tmp \
		./sblg -o- -j regress/json/*.xml 2>&1 >/dev/null ; \
	[ ! -s $$tmp ] || { \
		echo "regress/json/\*.xml... fail" ; \
		cat $$tmp ; \
		rm -f $$tmp ; \
		exit 1 ; \
	} ; \
	echo "regress/json/\*.xml... ok" ; \
	rm -f $$tmp

regress: all
	@tmp=`mktemp` ; \
	for f in regress/standalone/*.in.xml ; do \
		d=`dirname $$f` ; \
		tf=`basename $$f .in.xml`.template.xml ; \
		[ -f $$d/$$tf ] || tf=simple.template.xml ; \
		vf=`basename $$f .in.xml`.html ; \
		${REGRESS_ENV} ./sblg -o- -t $$d/$$tf -c $$f >$$tmp 2>/dev/null ; \
		diff $$tmp $$d/$$vf 2>/dev/null 1>&2 || { \
			echo "$$f... fail" ; \
			set +e ; \
			diff -u $$d/$$vf $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		} ; \
		echo "$$f... ok" ; \
	done ; \
	for f in regress/blog/*.in.xml ; do \
		d=`dirname $$f` ; \
		tf=`basename $$f .in.xml`.template.xml ; \
		[ -f $$d/$$tf ] || tf=simple.template.xml ; \
		vf=`basename $$f .in.xml`.html ; \
		${REGRESS_ENV} ./sblg -o- -t $$d/$$tf $$f >$$tmp 2>/dev/null ; \
		diff $$tmp $$d/$$vf 2>/dev/null 1>&2 || { \
			echo "$$f... fail" ; \
			set +e ; \
			diff -u $$d/$$vf $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		} ; \
		echo "$$f... ok" ; \
	done ; \
	for tf in regress/blog/*.template.xml ; do \
		d=`dirname $$tf` ; \
		f=`basename $$tf .template.xml`.in.xml ; \
		[ ! -f $$d/$$f ] || continue ; \
		f=$$d/simple.in.xml ; \
		vf=`basename $$tf .template.xml`.html ; \
		${REGRESS_ENV} ./sblg -o- -t $$tf $$f >$$tmp 2>/dev/null ; \
		diff $$tmp $$d/$$vf 2>/dev/null 1>&2 || { \
			echo "$$tf... fail" ; \
			set +e ; \
			diff -u $$d/$$vf $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		} ; \
		echo "$$tf... ok" ; \
	done ; \
	set +e ; \
	jq=`command -v $(JQ) 2>/dev/null` ; \
	set -e ; \
	if [ -n "$$jq" ]; then \
		${REGRESS_ENV} ./sblg -o- -j regress/json/*.xml | $$jq | \
			grep -v '"version":' > $$tmp ; \
		diff $$tmp regress/json/expect.json || { \
			echo "regress/json/expect.json... fail" ; \
			set +e ; \
			diff -u $$tmp regress/json/expect.json ; \
			rm -f $$tmp ; \
			exit 1 ; \
		} ; \
		echo "regress/json/expect.json... ok" ; \
	else \
		echo "regress/json/expect.json... skipping" ; \
	fi ; \
	rm -f $$tmp

distclean: clean
	rm -f Makefile.configure config.h config.log

coverage.xml::
	$(MAKE) clean
	CC=gcc CFLAGS="--coverage" ./configure LDFLAGS="--coverage"
	$(MAKE) regress
	@( echo '<?xml version="1.0" encoding="UTF-8" ?>'; \
	  echo '<article data-sblg-article="1" data-sblg-tags="coverage">'; \
	  echo '<aside>'; \
	  echo '<div class="coverage-table">' ; \
	  for f in $(OBJS) ; do \
	  	src=$$(basename $$f .o).c ; \
		link=https://github.com/kristapsdz/sblg/blob/master/$$src ; \
		pct=$$(gcov -H $$src | grep 'Lines executed' | head -n1 | \
			cut -d ":" -f 2 | cut -d "%" -f 1) ; \
	  	echo "<a href=\"$$link\">$$src</a><span>$$pct%</span>" ; \
		echo "$$src: $$pct%" 1>&2 ; \
	  done ; \
	  echo "</div>"; \
	  echo "</aside>"; \
	  echo "</article>"; \
	) >$@
