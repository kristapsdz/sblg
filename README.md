## Synopsis

[sblg](http://kristaps.bsd.lv/sblg) is a utility for
creating static blogs: it knits together articles and templates,
generating static HTML files and Atom feeds.
It's built for use with `make`, as blogs depend upon articles, etc. 
No markdown, no "CMS", no CGI, no PHP. Just a simple tool for pulling
data from articles and populating templates. 
The system is an ISC licensed ISO C utility that depends only on
[libexpat](http://expat.sourceforge.net/). 
(Where "simple" encompasses Atom feeds, multi-language support, tag
filtering, etc.) 

This is the README file for display with
[GitHub](https://www.github.com), which hosts a read-only source
repository of the project. 

## Examples

Read [sblg(1)](http://kristaps.bsd.lv/sblg/sblg.1.html) for a detailed
runthrough of operation, or visit the
[sblg blog](http://kristaps.bsd.lv/sblg/index.html#blog) for some
examples.

## Installation

sblg works out-of-the-box with modern UNIX systems.
Simply download the latest version's [source
archive](http://kristaps.bsd.lv/sblg/snapshots/sblg.tar.gz) (or download
the project from GitHub), compile with `make`, then `sudo make install`.
Your operating system might already have kcgi as one of its third-party
libraries: check to make sure!

## License

All sources use the ISC (like OpenBSD) license.
