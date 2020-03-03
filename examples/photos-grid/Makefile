.SUFFIXES: .jpg .xml

# If you want to use `make installwww` to publish your blog, put the
# publication point here.  I'll usually use `make` to put it all
# together, review, then `doas make installwww` to publish.

WWWDIR	 = /var/www/htdocs

# Add articles (really these are photos, e.g., photo1.jpg) here.

ARTICLES = photo1.xml \
	   photo2.xml \
	   photo3.xml \
	   photo4.xml \
	   photo5.xml \
	   photo6.xml \
	   photo7.xml \
	   photo8.xml
SBLG	 = sblg
PREFIX	 = /usr/local/share/sblg/examples/photos-grid

all: index.html atom.xml

installwww: all
	mkdir -p $(WWWDIR)
	install -m 0444 atom.xml *.html *.css photo[0-9*].jpg photo[0-9*]-thumb.jpg $(WWWDIR)

install:
	mkdir -p $(PREFIX)
	install -m 0444 Makefile atom-template.xml index.xml photo.xml *.md *.css photo[1-8].jpg $(PREFIX)

index.html: $(ARTICLES) index.xml photo.xml
	sblg -o $@ -t index.xml $(ARTICLES)
	sblg -t photo.xml -L $(ARTICLES)

clean:
	rm -f *-thumb.jpg $(ARTICLES) index.html photo*.html atom.xml

atom.xml: $(ARTICLES) atom-template.xml
	$(SBLG) -s rcmdline -o $@ -a $(ARTICLES)

.jpg.xml:
	convert $< -resize 400x400 -quality 40 `basename $< .jpg`-thumb.jpg
	( echo -n "<article data-sblg-article=\"1\" " ; \
	  exiftool -sort -e -ex $< | while read ln ; do \
		key=`echo $$ln | cut -d':' -f1 | sed 's![ /]!!g'`; \
		val=`echo $$ln | cut -d':' -f2- | sed 's!^[ ]*!!'` ; \
		echo ; \
		echo -n " data-sblg-set-$$key=\"$$val\"" ; \
		done ; \
	  echo ">" ; \
	  echo " <h1>`exiftool -ex -Title -s3 $<`</h1>" ; \
	  echo " <address>`exiftool -ex -Artist -s3 $<`</address>" ; \
	  echo " <aside>`exiftool -ex -Description -s3 $<`</aside>" ; \
	  echo " <time datetime=\"`exiftool -ex -DateTimeOriginal -d '%Y-%m-%dT%H:%M:%SZ' -s3 $<`\">" ; \
	  echo "  `exiftool -ex -d '%Y-%m-%d %H:%M:%S' -DateTimeOriginal -s3 $< `" ; \
	  echo " </time>" ; \
	  echo " <img src=\"$<\" />" ; \
	  echo "</article>" ; \
	) >$@
