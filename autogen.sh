#!/bin/sh

PKG_NAME="indicator-bluetooth"

which gnome-autogen.sh || {
	echo "You need gnome-common"
	exit 1
}

USE_GNOME2_MACROS=1 \
. gnome-autogen.sh

