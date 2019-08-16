.SUFFIXES: .md .xml

# If you want to use `make installwww` to publish your blog, put the
# publication point here.  I'll usually use `make` to put it all
# together, review, then `doas make installwww` to publish.

WWWDIR	 = /var/www/htdocs

# Add articles here.  For simplicity with linking `index.html` to the
# newest one, the order is publication order.

ARTICLES = article1.xml \
	   article2.xml
SBLG	 = sblg
PREFIX	 = /usr/local/share/sblg/examples/retro

all: index.html atom.xml

installwww: all
	mkdir -p $(WWWDIR)
	install -m 0444 atom.xml *.{html,css} $(WWWDIR)

install: all
	mkdir -p $(PREFIX)
	install -m 0444 Makefile atom-template.xml index.xml template.xml *.{md,css} $(PREFIX)

index.html: $(ARTICLES) template.xml index.xml
	$(SBLG) -s cmdline -t template.xml -L $(ARTICLES)
	$(SBLG) -s rcmdline -t index.xml -o $@ $(ARTICLES)

atom.xml: $(ARTICLES) atom-template.xml
	$(SBLG) -s rcmdline -o $@ -a $(ARTICLES)

clean:
	rm -f *.html article*.xml atom.xml

.md.xml:
	echo "<article data-sblg-article=\"1\"" >$@
	echo " data-sblg-tags=\"`lowdown -X tags $<`\">" >> $@
	echo " <header>" >>$@
	echo "  <h2>`lowdown -X title $<`</h2>" >>$@
	echo "  <aside>`lowdown -X aside $<`</aside>" >>$@
	echo "  <address>`lowdown -X author $<`</address>" >>$@
	echo "  <time datetime=\"`lowdown -X date $<`\">`lowdown -X date $<`</time>" >>$@
	echo " </header>" >>$@
	lowdown $< >>$@
	echo "</article>" >>$@
