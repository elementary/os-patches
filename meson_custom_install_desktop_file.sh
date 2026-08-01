#! /bin/sh

set -eu

applicationsdir=$1
sysconfdir=$2

mkdir -p "${DESTDIR:-}$applicationsdir/"
cp "${DESTDIR:-}$sysconfdir/user-dirs-update-gtk.desktop" \
   "${DESTDIR:-}$applicationsdir/user-dirs-update-gtk.desktop"

