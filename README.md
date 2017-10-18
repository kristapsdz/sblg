## Synopsis

[sblg](http://kristaps.bsd.lv/sblg) is a utility for creating static
blogs. 
It knits articles together with templates, generating static HTML
files, Atom feeds, and JSON files.
It's built for use with `make`-style build environments insofar as a
blog depends upon articles.

Benefits?  
No markdown, no "CMS", no CGI, no PHP. 
Just a simple [open source](https://opensource.org/licenses/ISC) tool
for pulling data from articles and populating templates. 
The only dependency is [libexpat](http://expat.sourceforge.net/) for
parsing article content.

This GitHub repository is a read-only mirror of the main repository,
which is held on [BSD.lv](https://www.bsd.lv).
I keep it up to date between versions; so if you have issues to report,
please do so here.

## Examples

Read [sblg(1)](http://kristaps.bsd.lv/sblg/sblg.1.html) for a detailed
runthrough of operation, or visit the
[sblg blog](http://kristaps.bsd.lv/sblg/index.html#blog) for some
examples.

## Installation

sblg works out-of-the-box with modern UNIX systems.
Simply download the latest version's [source
archive](http://kristaps.bsd.lv/sblg/snapshots/sblg.tar.gz) (or download
the project from GitHub), compile with `make`, then `sudo make install`
(or `doas make install`, if you're on OpenBSD).

## License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
