.SUFFIXES: .md .xml

# If you want to use `make install` to publish your blog, put the
# publication point here.  I'll usually use `make` to put it all
# together, review, then `doas make install` to publish.

PREFIX	 = /var/www/htdocs
SBLG	 = sblg

# Add articles here.  For simplicity with linking `index.html` to the
# newest one, the order is publication order.

ARTICLES = article1.xml \
	   article2.xml

all: index.html atom.xml

install: all
	mkdir -p $(PREFIX)
	install -m 0444 atom.xml *.html *.css $(PREFIX)

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
	echo "  <time datetime=\"`lowdown -X date $<`\">`lowdown -X date $<`</time>" >>$@
	echo " </header>" >>$@
	lowdown $< >>$@
	echo "</article>" >>$@
