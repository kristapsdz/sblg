# If you want to use `make install` to publish your blog, put the
# publication point here.  I'll usually use `make` to put it all
# together, review, then `doas make installwww` to publish.

WWWDIR	 = /var/www/htdocs

# Add articles here.  For simplicity with linking `index.html` to the
# newest one, the order is publication order.

ARTICLES = article1.xml \
	   article2.xml \
	   article3.xml
SBLG	 = sblg
PREFIX	 = /usr/local/share/sblg/examples/simple-frontpage

all: index.html atom.xml

installwww: all
	mkdir -p $(WWWDIR)
	install -m 0444 atom.xml *.html *.css *.jpg $(WWWDIR)

install:
	mkdir -p $(PREFIX)
	install -m 0444 Makefile atom-template.xml article*.xml index.xml template.xml *.md *.css *.jpg $(PREFIX)

index.html: $(ARTICLES) template.xml index.xml
	$(SBLG) -s cmdline -t template.xml -L $(ARTICLES)
	$(SBLG) -s rcmdline -t index.xml -o $@ $(ARTICLES)

atom.xml: $(ARTICLES) atom-template.xml
	$(SBLG) -s rcmdline -o $@ -a $(ARTICLES)

clean:
	rm -f *.html atom.xml
