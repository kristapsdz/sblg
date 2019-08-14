# If you want to use `make install` to publish your blog, put the
# publication point here.  I'll usually use `make` to put it all
# together, review, then `doas make install` to publish.

PREFIX	 = /var/www/htdocs
SBLG	 = sblg

# Add articles here.  For simplicity with linking `index.html` to the
# newest one, the order is publication order.

ARTICLES = article1.xml \
	   article2.xml \
	   article3.xml

all: index.html atom.xml

install: all
	mkdir -p $(PREFIX)
	install -m 0444 atom.xml *.html *.css *.jpg $(PREFIX)

index.html: $(ARTICLES) template.xml
	$(SBLG) -s cmdline -t template.xml -L $(ARTICLES)
	ln -f $$(basename $$(ls $(ARTICLES) | tail -n1) .xml).html $@

atom.xml: $(ARTICLES) atom-template.xml
	$(SBLG) -s rcmdline -o $@ -a $(ARTICLES)

clean:
	rm -f *.html atom.xml
