#!/bin/sh -e

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

(test -f $srcdir/libaccounts-glib.pc.in ) || {
	echo -n "Error: Directory "\`$srcdir\`" does not look like the "
        echo "top-level libaccounts-glib directory."
	exit 1
}

cd "$srcdir"
gtkdocize --copy --flavour no-tmpl
cd "$OLDPWD"
autoreconf --install --force --verbose --warnings=all "$srcdir"
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
