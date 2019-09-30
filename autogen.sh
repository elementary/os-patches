#!/bin/sh -e

mkdir -p m4
autoreconf -i
intltoolize -c -f
test -n "$NOCONFIGURE" || ./configure "$@"
