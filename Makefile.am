SUBDIRS = src themes images scripts systemd-units po

if BUILD_DOCUMENTATION
SUBDIRS += docs
endif

DISTCHECK_CONFIGURE_FLAGS = --disable-tests --disable-systemd-integration

EXTRA_DIST = build-tools/config.rpath ChangeLog                                \
             README

MAINTAINERCLEANFILES = aclocal.m4                                              \
                       config.h.in                                             \
                       config.h.in~                                            \
                       config.sub                                              \
                       configure                                               \
                       Makefile.in

ACLOCAL_AMFLAGS = -I m4
