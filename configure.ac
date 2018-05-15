AC_PREREQ(2.53)

AC_INIT([xdg-user-dirs-gtk], [0.10],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=xdg-user-dirs-gtk])
AC_CONFIG_SRCDIR([parse.c])
AM_INIT_AUTOMAKE([1.9 dist-xz no-dist-gzip tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])

AM_SANITY_CHECK
AM_MAINTAINER_MODE

AC_C_CONST
AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

GETTEXT_PACKAGE=xdg-user-dirs-gtk
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The gettext domain name])
AC_SUBST(GETTEXT_PACKAGE)

AM_GLIB_GNU_GETTEXT
GLIB_DEFINE_LOCALEDIR(GLIBLOCALEDIR)

IT_PROG_INTLTOOL([0.35.0])
	 
PKG_CHECK_MODULES(GTK, gtk+-3.0)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AC_PATH_PROG(XDG_USER_DIRS_UPDATE, xdg-user-dirs-update, no)
if test "x${XDG_USER_DIRS_UPDATE}" = "xno"; then
  AC_MSG_ERROR([You need to install xdg-user-dirs])
fi
	
dnl ==========================================================================

dnl Turn on the additional warnings last, so -Werror doesn't affect other tests.

AC_ARG_ENABLE(more-warnings,
[  --enable-more-warnings  Maximum compiler warnings],
set_more_warnings="$enableval",[
if test -f $srcdir/CVSVERSION; then
	is_cvs_version=true
	set_more_warnings=yes
else
	set_more_warnings=no
fi
])
AC_MSG_CHECKING(for more warnings, including -Werror)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
	AC_MSG_RESULT(yes)
	CFLAGS="\
	-Wall \
	-Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
	-Wnested-externs -Wpointer-arith \
	-Wcast-align -Wsign-compare \
	-Werror \
	$CFLAGS"

	for option in -Wno-strict-aliasing -Wno-sign-compare; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		AC_TRY_COMPILE([], [],
			has_option=yes,
			has_option=no,)
		if test $has_option = no; then
			CFLAGS="$SAVE_CFLAGS"
		fi
		AC_MSG_RESULT($has_option)
		unset has_option
		unset SAVE_CFLAGS
	done
	unset option
else
	AC_MSG_RESULT(no)
fi
			       
AC_OUTPUT([
Makefile
po/Makefile.in
])
