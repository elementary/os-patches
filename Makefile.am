ACLOCAL_AMFLAGS = -I m4

include $(top_srcdir)/Makefile.am.coverage

SUBDIRS = \
  lib/libbamf-private \
  lib/libbamf \
  src \
  tests \
  data \
  doc

EXTRA_DIST = COPYING.LGPL autogen.sh build.rules.mk
DISTCHECK_CONFIGURE_FLAGS = --enable-introspection=no --enable-gtk-doc=yes

