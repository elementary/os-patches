dnl GLIB_CONFIG([MINIMUM-VERSION, [, MODULES]])
dnl Test for GLIB (and error out if it's not available). Define
dnl GLIB_CFLAGS and GLIB_LIBS, GLIB_MAKEFILE, and variables for
dnl various glib developer tools (eg, GLIB_GENMARSHAL). If
dnl gmodule, gobject, or gio is specified in MODULES, pass to
dnl pkg-config
dnl
AC_DEFUN([GLIB_CONFIG_NMA],
[dnl
  min_glib_version=ifelse([$1], ,2.26.0,$1)
  pkg_config_args=
  for module in glib $2; do
    pkg_config_args="$pkg_config_args $module-2.0 >= $min_glib_version"
  done

  PKG_CHECK_MODULES(GLIB, $pkg_config_args)

  GLIB_CFLAGS="$GLIB_CFLAGS -DG_DISABLE_SINGLE_INCLUDES"

  GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
  GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`

  #GLIB_MAKEFILE=`$PKG_CONFIG --variable=glib_makefile glib-2.0`
  GLIB_MAKEFILE='$(top_srcdir)/Makefile.glib'

  AC_SUBST(GLIB_GENMARSHAL)
  AC_SUBST(GLIB_MKENUMS)
  AC_SUBST(GLIB_MAKEFILE)

  GLIB_GSETTINGS
])
