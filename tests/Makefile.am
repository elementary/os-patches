VER=3
IDOLIB = $(top_builddir)/src/libido3-0.1.la

check_LIBRARIES = libgtest.a
check_PROGRAMS = 
TESTS =

AM_CPPFLAGS = \
	$(GTEST_CPPFLAGS) \
	-I${top_srcdir}/src

#############################
# Google Test base library
#############################

nodist_libgtest_a_SOURCES = \
	$(XORG_GTEST_SOURCE)/src/xorg-gtest-all.cpp \
	$(GTEST_SOURCE)/gtest-all.cc \
	$(XORG_GTEST_SOURCE)/src/xorg-gtest_main.cpp
libgtest_a_CPPFLAGS = \
	$(XORG_GTEST_CPPFLAGS) \
	$(AM_CPPFLAGS) \
	$(GTEST_CPPFLAGS) -w
libgtest_a_CXXFLAGS = \
	$(AM_CXXFLAGS)

#############################
# Menuitem tests
#############################

TESTS += gtest-menuitems
check_PROGRAMS += gtest-menuitems

gtest_menuitems_SOURCES = \
	gtest-menuitems.cpp
gtest_menuitems_CPPFLAGS = \
	$(GCC_CFLAGS) \
	$(GTK_CFLAGS) \
	$(MAINTAINER_CFLAGS) \
	$(AM_CPPFLAGS)
gtest_menuitems_LDFLAGS = \
	-pthread
gtest_menuitems_LDADD = \
	$(GTK_LIBS) \
	$(IDOLIB) \
	libgtest.a \
	-lX11 -lXi

