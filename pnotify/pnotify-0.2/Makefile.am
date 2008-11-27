#		$Id: $		

#
# Copyright (c) 2007 Mark Heily <devel@heily.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

libpnotify_a_SOURCES=	pnotify.c
include_HEADERS=	pnotify.h
noinst_HEADERS=		queue.h
lib_LIBRARIES=		libpnotify.a
libpnotify_a_CFLAGS=	-O2 -g -Wall
dist_man3_MANS=		pnotify.3
EXTRA_DIST=		index.html 

#
# Unit testing program
# 
TESTS=			test
check_PROGRAMS=		test
test_SOURCES=		test.c pnotify.c
test_CFLAGS=		-DPNOTIFY_DEBUG=1

# Preview the manpage in the current terminal window
preview-man:
	nroff -Tascii -mandoc pnotify.3 | less

# The 'doc' target generates additional documentation
doc:
	groff -Thtml -mandoc pnotify.3 > manpage.html
	groff -Tps -mandoc pnotify.3 > manpage.ps
	lynx -dump index.html > README

# The 'sync' target rebuilds the webpage and uploads it to the public server

sync:		doc
	scp index.html manpage.html manpage.ps $$WWWHOST:~/public_html/pnotify

# Ensure that the documentation is regenerated when building a source tarball
dist-hook:	doc
