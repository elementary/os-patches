#!/bin/sh

gcc -o indicator-messages-service-activate indicator-messages-service-activate.c `pkg-config --cflags --libs dbus-1 dbus-glib-1`
