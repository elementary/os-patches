
check_LIBRARIES = libgtest.a
check_PROGRAMS = test-gactionmuxer

TESTS = $(check_PROGRAMS)

AM_CPPFLAGS = $(GTEST_CPPFLAGS) \
	-I${top_srcdir}/src

nodist_libgtest_a_SOURCES = \
	$(GTEST_SOURCE)/src/gtest-all.cc \
	$(GTEST_SOURCE)/src/gtest_main.cc
libgtest_a_CPPFLAGS = \
	$(GTEST_CPPFLAGS) -w \
	$(AM_CPPFLAGS)
libgtest_a_CXXFLAGS = \
	$(AM_CXXFLAGS)


test_gactionmuxer_SOURCES = \
	test-gactionmuxer.cpp

test_gactionmuxer_CPPFLAGS = \
	$(APPLET_CFLAGS) \
	$(AM_CPPFLAGS)

test_gactionmuxer_LDADD = \
	$(APPLET_LIBS) \
	libindicator-messages-service.la \
	libgtest.a


######################################
# Lib containing code under test
######################################

noinst_LTLIBRARIES = \
	libindicator-messages-service.la

libindicator_messages_service_la_SOURCES = \
	$(top_builddir)/common/indicator-messages-service.c \
	$(top_builddir)/common/indicator-messages-service.h \
	$(top_srcdir)/src/gactionmuxer.c \
	$(top_srcdir)/src/gactionmuxer.h
	$(top_srcdir)/src/dbus-data.h

libindicator_messages_service_ladir = \
	$(includedir)/libindicator-messages-service/

libindicator_messages_service_la_CFLAGS = \
	$(APPLET_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-I$(top_builddir)/src \
	-I$(top_builddir)/common \
	-Wall \
	-Wl,-Bsymbolic-functions \
	-Wl,-z,defs \
	-Wl,--as-needed \
	-Werror -Wno-error=deprecated-declarations \
	-DG_LOG_DOMAIN=\"Indicator-Messages\"

libindicator_messages_service_la_LIBADD = \
	$(APPLET_LIBS)

libindicator_messages_service_la_LDFLAGS = \
	$(COVERAGE_LDFLAGS)

######################################
# Test client with dbusmock
######################################

TESTS_ENVIRONMENT = \
	export LD_LIBRARY_PATH=$(top_builddir)/libmessaging-menu/.libs; \
	export GI_TYPELIB_PATH=$(top_builddir)/libmessaging-menu; \
	export XDG_DATA_DIRS=$(abs_srcdir);

TESTS += test-client.py
