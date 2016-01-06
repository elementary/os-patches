CC=gcc
PKG_CONFIG=pkg-config
CFLAGS := $(shell getconf LFS_CFLAGS) -Wall -O2 -g $(shell $(PKG_CONFIG) --cflags ply-boot-client)
LDLIBS=$(shell $(PKG_CONFIG) --libs ply-boot-client)

casper-md5check: casper-md5check.c md5.c

clean:
	rm -f *.o casper-md5check
