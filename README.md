# nroff_man
nroff -man; man without nroff.

This is a manual page viewer. It was written as a replacement for [GNO/ME](https://github.com/GnoConsortium/gno)'s
nroff-based man page viewer, but should work elsewhere (developed with clang on OS X and gcc on Windows/MSYS2).

The goal is to support the man macros (.TH, .SH, ...) directly, as well as a reasonable enough subset of troff commands
and escape codes. Other goals:

* fast
* small stack usage
* bug free
* pretty output

Ideally, man pages will be written in mandoc, converted to man via the [mandoc](http://mandoc.bsd.lv) utility,
then displayed with nroff_man.
