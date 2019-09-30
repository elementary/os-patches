#!/bin/sh

PKG_NAME="ido"

which gnome-autogen.sh || {
    echo "You need gnome-common from GNOME git"
    exit 1
}

USE_GNOME2_MACROS=1 \
USE_COMMON_DOC_BUILD=yes \
gnome-autogen.sh --enable-gtk-doc $@