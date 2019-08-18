This is a different form of template.  It's built for a photo gallery,
directly from the photos themselves.  It uses
[exiftool](https://www.sno.phy.queensu.ca/~phil/exiftool/) to create
article files, extracting pertinent information from the photos.

To preserve bandwidth, photos on the main blog are shown as an "infinite
scroll", where only photos in-frame are rendered.

As usual, it uses [make](https://man.openbsd.org/make.1) to generate
HTML5 articles from JPG sources.  The rendered output is visible at
https://kristaps.bsd.lv/sblg/examples/photo.

It has the following features:

- super simple layout (mobile friendly)
- lightweight (only a CSS file, JS embedded in the HTML)
- direct from photos
- Atom feed

It requires the newest version of [sblg](https://kristaps.bsd.lv/sblg).
To use:

1. Append photos (as XML files) to the `XMLS` variable in the
	[Makefile](Makefile) in any order.
2. Edit [template.xml](template.xml) with the proper name of your blog
	(the example is "Photo Template Blog") and proper license.
3. Edit [index.xml](index.xml) with the proper name and description of
	your blog (the example is "Photo Template Blog") and proper
	license.
4. Edit the URLs in [atom-template.xml](atom-template.xml).
5. To directly publish, modify the [Makefile](Makefile)'s `WWWDIR`
	variable to a web server root and run `make installwww`.  Or
	install into a staging directory and copy from there.
6. Make sure that your photos have their EXIF tags intact, and if
	desired, fill in the title, comments ("description") and artist
	EXIF tags in your favourite editor.  I use
	[geeqie](http://www.geeqie.org/).

I've included several of my own photos as examples.  These are CC-BY-SA
4.0.  I don't recommend using them, as they're low quality to keep the
repository small.
