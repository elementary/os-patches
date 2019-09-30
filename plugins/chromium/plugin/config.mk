VERSION = 0.3

# Customize below to fit your system

INCS = `pkg-config --libs --cflags libxul nspr libbamf`
CPPFLAGS = -DVERSION=\"${VERSION}\" -DXULRUNNER_SDK
CFLAGS = -g -pedantic -fPIC -Wall -O2 ${INCS} ${CPPFLAGS}

# compiler and linker
CC = gcc
