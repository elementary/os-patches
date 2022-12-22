AC_INIT([plymouth],[0.9.5+git20211018],[https://gitlab.freedesktop.org/plymouth/plymouth/issues])
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_HEADER(config.h)
AC_CONFIG_AUX_DIR(build-tools)

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_PROG_AWK
AC_PROG_CC_STDC
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_C_CONST

AM_INIT_AUTOMAKE([dist-xz no-dist-gzip subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

PKG_PROG_PKG_CONFIG
LT_INIT([dlopen disable-static pic-only])

## increment if the interface has additions, changes, removals.
LT_CURRENT=5

## increment any time the source changes; set to
##  0 if you increment CURRENT
LT_REVISION=0

## increment if any interfaces have been added; set to 0
## if any interfaces have been changed or removed. removal has
## precedence over adding, so set to 0 if both happened.
LT_AGE=0

AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

PKG_CHECK_MODULES(IMAGE, [libpng >= 1.2.16 ]) 
AC_SUBST(IMAGE_CFLAGS)
AC_SUBST(IMAGE_LIBS)

AC_ARG_WITH(udev, AS_HELP_STRING([--with-udev], [Add udev support]),, with_udev=yes)

if test "x$with_udev" != "xno" ; then
  PKG_CHECK_MODULES(UDEV, [libudev], have_udev=yes, have_udev=no)
  AC_SUBST(UDEV_CFLAGS)
  AC_SUBST(UDEV_LIBS)
  if test "x$have_udev" = "xyes"; then
    AC_DEFINE(HAVE_UDEV, 1, [Define if have udev support])
  else
    AC_MSG_ERROR([libudev is required unless --without-udev is passed])
  fi
fi

PLYMOUTH_CFLAGS=""
PLYMOUTH_LIBS="-lm -lrt -ldl"

AC_SUBST(PLYMOUTH_CFLAGS)
AC_SUBST(PLYMOUTH_LIBS)

AC_PATH_PROG([SYSTEMD_ASK_PASSWORD_AGENT], [systemd-tty-ask-password-agent])
AC_PATH_PROG([UDEVADM], [udevadm])

AC_ARG_ENABLE(pango, AS_HELP_STRING([--enable-pango],[enable building with pango, disabled there is no encryption prompts]),enable_pango=$enableval,enable_pango=yes)
AM_CONDITIONAL(ENABLE_PANGO,  [test "$enable_pango" = yes])

if test x$enable_pango = xyes; then
  PKG_CHECK_MODULES(PANGO, [pangocairo >= 1.21.0 ])
  AC_SUBST(PANGO_CFLAGS)
  AC_SUBST(PANGO_LIBS)
fi

AC_ARG_ENABLE(gtk, AS_HELP_STRING([--enable-gtk],[enable building with gtk, disabled there is no x11 renderer]),enable_gtk=$enableval,enable_gtk=yes)
AM_CONDITIONAL(ENABLE_GTK,  [test "$enable_gtk" = yes])

if test x$enable_gtk = xyes; then
  PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= 3.14.0])
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)
fi

AC_ARG_ENABLE(drm, AS_HELP_STRING([--enable-drm],[enable building drm kms support]),enable_drm_renderer=$enableval,enable_drm_renderer=yes)
AM_CONDITIONAL(ENABLE_DRM_RENDERER,  [test "$enable_drm_renderer" = yes])

if test x$enable_drm_renderer = xyes; then
  PKG_CHECK_MODULES(DRM, [libdrm])
fi

AC_ARG_ENABLE(documentation,
              AS_HELP_STRING([--enable-documentation],
                             [build documentation]),,
              enable_documentation=yes)
if test x$enable_documentation = xyes; then
   AC_PATH_PROG([XSLTPROC], [xsltproc])
   if test x$XSLTPROC = x; then
      AC_MSG_ERROR([xsltproc is required to build documentation])
   fi
fi
AM_CONDITIONAL(BUILD_DOCUMENTATION, test x$enable_documentation = xyes)

AC_ARG_ENABLE(tracing, AS_HELP_STRING([--enable-tracing],[enable verbose tracing code]),enable_tracing=$enableval,enable_tracing=yes)

if test x$enable_tracing = xyes; then
  AC_DEFINE(PLY_ENABLE_TRACING, 1, [Build in verbose debug tracing spew])
fi

AC_ARG_ENABLE(upstart-monitoring, AS_HELP_STRING([--enable-upstart-monitoring],[listen for messages on the Upstart D-Bus interface]),enable_upstart_monitoring=$enableval,enable_upstart_monitoring=no)
if test x$enable_upstart_monitoring = xyes; then
  PKG_CHECK_MODULES(DBUS, [dbus-1])
  AC_SUBST(DBUS_CFLAGS)
  AC_SUBST(DBUS_LIBS)
  AC_CHECK_HEADERS([ncursesw/term.h ncurses/term.h term.h], [break])
  AC_CHECK_LIB([ncursesw], [initscr],
    [CURSES_LIBS="$CURSES_LIBS -lncursesw"],
    [AC_CHECK_LIB([ncurses], [initscr],
      [CURSES_LIBS="$CURSES_LIBS -lncurses"],
      [AC_CHECK_LIB([curses], [initscr],
        [CURSES_LIBS="$CURSES_LIBS -lcurses"],
        [AC_MSG_ERROR([no curses library found])])])])
  AC_SUBST(CURSES_LIBS)
fi
AM_CONDITIONAL(ENABLE_UPSTART_MONITORING, [test "$enable_upstart_monitoring" = yes])

AC_ARG_ENABLE(systemd-integration, AS_HELP_STRING([--enable-systemd-integration],[coordinate boot up with systemd]),enable_systemd_integration=$enableval,enable_systemd_integration=yes)
AM_CONDITIONAL(ENABLE_SYSTEMD_INTEGRATION, [test "$enable_systemd_integration" = yes])

if test x$enable_systemd_integration = xyes; then
  AC_DEFINE(PLY_ENABLE_SYSTEMD_INTEGRATION, 1, [Coordinate boot up with systemd])
  AC_ARG_WITH([systemdunitdir], AC_HELP_STRING([--with-systemdunitdir=DIR],
    [path to systemd service directory]), [path_systemdunit=${withval}],
      [path_systemdunit="`$PKG_CONFIG --variable=systemdsystemunitdir systemd`"])
  if (test -n "${path_systemdunit}"); then
    SYSTEMD_UNIT_DIR="${path_systemdunit}"
  fi
  AC_SUBST(SYSTEMD_UNIT_DIR)
fi

AC_ARG_WITH([runtimedir],
            AC_HELP_STRING([--with-runtimedir=DIR], [runtime data dir [LOCALSTATEDIR/run]]),
            [plymouthruntimedir=${withval}/plymouth], [plymouthruntimedir=""])

AC_ARG_WITH(system-root-install, AS_HELP_STRING([--with-system-root-install],[Install client in /bin and daemon in /sbin]),with_system_root_install=${withval},with_system_root_install=no)
AM_CONDITIONAL(WITH_SYSTEM_ROOT_INSTALL,  [test "$with_system_root_install" = yes])

if test x$with_system_root_install = xyes; then
  plymouthclientdir=/bin
  plymouthdaemondir=/sbin
  if (test -z "${plymouthruntimedir}"); then
    plymouthruntimedir=/run/plymouth
  fi
else
  plymouthclientdir=$bindir
  plymouthdaemondir=$sbindir
  if (test -z "${plymouthruntimedir}"); then
    plymouthruntimedir=$localstatedir/run/plymouth
  fi
fi
AC_SUBST(plymouthclientdir)
AC_SUBST(plymouthdaemondir)
AC_SUBST(plymouthruntimedir)

AS_AC_EXPAND(PLYMOUTH_CLIENT_DIR, $plymouthclientdir)
AS_AC_EXPAND(PLYMOUTH_DAEMON_DIR, $plymouthdaemondir)
AS_AC_EXPAND(PLYMOUTH_RUNTIME_DIR, $plymouthruntimedir)

AC_ARG_WITH(rhgb-compat-link, AS_HELP_STRING([--with-rhgb-compat-link],[Install /usr/bin/rhgb-client compatability symlink]),with_rhgb_compat_link=${withval},with_rhgb_compat_link=no)
AM_CONDITIONAL(WITH_RHGB_COMPAT_LINK,  [test "$with_rhgb_compat_link" = yes])

AC_ARG_WITH(logo, AS_HELP_STRING([--with-logo],[logo used by boot splash plugins]),logofile=${withval},logofile=$datadir/plymouth/bizcom.png)
AC_SUBST(logofile)
AS_AC_EXPAND(PLYMOUTH_LOGO_FILE, $logofile)

AM_CONDITIONAL(INSTALL_FALLBACK_LOGO,  [test "$logofile" = $datadir/plymouth/bizcom.png])

AC_ARG_WITH(background-color, AS_HELP_STRING([--with-background-color],[background color used by boot splash plugins]),background_color=${withval},background_color=0x5d5950)
AC_SUBST(background_color)

AC_ARG_WITH(background-start-color-stop, AS_HELP_STRING([--with-background-start-color-stop],[first color stop in background gradients used by boot splash plugins]),background_start_color=${withval},background_start_color=0x807c71)
AC_SUBST(background_start_color)

AC_ARG_WITH(background-end-color-stop, AS_HELP_STRING([--with-background-end-color-stop],[first color end in background gradients used by boot splash plugins]),background_end_color=${withval},background_end_color=0x3a362f)
AC_SUBST(background_end_color)

AC_ARG_WITH(release-file, AS_HELP_STRING([--with-release-file=<path_to_release_file>],[Release File to use to detect distribution (by default /etc/system-release)]),RELEASE_FILE=${withval},RELEASE_FILE=/etc/system-release)

AC_SUBST(RELEASE_FILE)
AC_DEFINE_UNQUOTED(RELEASE_FILE, "$RELEASE_FILE", [Release file path])

AC_ARG_WITH(boot-tty, AS_HELP_STRING([--with-boot-tty=<tty>],[Default TTY to use in boot mode (by default tty1)]),BOOT_TTY=${withval},BOOT_TTY=/dev/tty1)
AC_DEFINE_UNQUOTED(BOOT_TTY, "$BOOT_TTY", [TTY to use in boot mode])

AC_ARG_WITH(shutdown-tty, AS_HELP_STRING([--with-shutdown-tty=<tty>],[Default TTY to use in shutdown mode (by default tty63)]),SHUTDOWN_TTY=${withval},SHUTDOWN_TTY=/dev/tty63)
AC_DEFINE_UNQUOTED(SHUTDOWN_TTY, "$SHUTDOWN_TTY", [TTY to use in shutdown mode])

# Turn on the additional warnings last, so -Werror doesn't affect other tests.

AC_DEFUN([PLYMOUTH_CC_TRY_FLAG], [
  AC_MSG_CHECKING([whether $CC supports $1])

  plymouth_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $1"

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ ]], [[ ]])], [plymouth_cc_flag=yes], [plymouth_cc_flag=no])
  CFLAGS="$plymouth_save_CFLAGS"

  if test "x$plymouth_cc_flag" = "xyes"; then
    ifelse([$2], , :, [$2])
  else
    ifelse([$3], , :, [$3])
  fi
  AC_MSG_RESULT([$plymouth_cc_flag])
])

AC_ARG_ENABLE(more-warnings,
              AS_HELP_STRING([--enable-more-warnings],
                             [Maximum compiler warnings]),
              set_more_warnings="$enableval",[
	      if test -d $srcdir/.git; then
	        set_more_warnings=yes
	      else
	        set_more_warnings=no
              fi])

AC_MSG_CHECKING(for more warnings)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
  AC_MSG_RESULT(yes)
  MAYBE_WARN="\
   -Wall -Wextra  \
   -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
   -Wwrite-strings -Wnested-externs -Wpointer-arith \
   -Wswitch-enum -Wstrict-aliasing=2 -Winit-self -Wunsafe-loop-optimizations \
   -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-result \
   -Wno-cast-function-type \
   -Wcast-align -Wsign-compare -Wp,-D_FORTIFY_SOURCE=2"
elif test "$GCC" = "yes"; then
  AC_MSG_RESULT(no)
  MAYBE_WARN="-Wall -Wno-sign-compare -Wno-deprecated-declarations"
else
  AC_MSG_RESULT(no)
fi

# invalidate cached value if MAYBE_WARN has changed
if test "x$plymouth_cv_warn_maybe" != "x$MAYBE_WARN"; then
	unset plymouth_cv_warn_cflags
fi
AC_CACHE_CHECK([for supported warning flags], plymouth_cv_warn_cflags, [
	echo
	WARN_CFLAGS=""

	# Some warning options are not supported by all versions of
	# gcc, so test all desired options against the current
	# compiler.
	#
	# Note that there are some order dependencies
	# here. Specifically, an option that disables a warning will
	# have no net effect if a later option then enables that
	# warnings, (perhaps implicitly). So we put some grouped
	# options (-Wall and -Wextra) up front and the -Wno options
	# last.

	for W in $MAYBE_WARN; do
		PLYMOUTH_CC_TRY_FLAG([$W], [WARN_CFLAGS="$WARN_CFLAGS $W"])
	done

	plymouth_cv_warn_cflags=$WARN_CFLAGS
	plymouth_cv_warn_maybe=$MAYBE_WARN

	AC_MSG_CHECKING([which warning flags were supported])])

WARN_CFLAGS="$plymouth_cv_warn_cflags"
PLYMOUTH_CFLAGS="$PLYMOUTH_CFLAGS $WARN_CFLAGS"

plymouththemedir=$datadir/plymouth/themes
AS_AC_EXPAND(PLYMOUTH_THEME_PATH, $plymouththemedir)

plymouthruntimethemedir=$plymouthruntimedir/themes
AS_AC_EXPAND(PLYMOUTH_RUNTIME_THEME_PATH, $plymouthruntimethemedir)

plymouthplugindir=$libdir/plymouth/
AS_AC_EXPAND(PLYMOUTH_PLUGIN_PATH, $plymouthplugindir)

plymouthpolicydir=$datadir/plymouth/
AS_AC_EXPAND(PLYMOUTH_POLICY_DIR, $plymouthpolicydir)

plymouthconfdir=$sysconfdir/plymouth/
AS_AC_EXPAND(PLYMOUTH_CONF_DIR, $plymouthconfdir)

AS_AC_EXPAND(PLYMOUTH_LIBDIR, $libdir)
AS_AC_EXPAND(PLYMOUTH_LIBEXECDIR, $libexecdir)
AS_AC_EXPAND(PLYMOUTH_DATADIR, $datadir)

AC_CONFIG_FILES([Makefile po/Makefile.in
           src/libply/Makefile
           src/libply-splash-core/Makefile
           src/libply-splash-graphics/Makefile
           src/ply-splash-core.pc
           src/ply-splash-graphics.pc
           src/plugins/Makefile
           src/plugins/renderers/Makefile
           src/plugins/renderers/frame-buffer/Makefile
           src/plugins/renderers/drm/Makefile
           src/plugins/renderers/x11/Makefile
           src/plugins/splash/Makefile
           src/plugins/splash/fade-throbber/Makefile
           src/plugins/splash/tribar/Makefile
           src/plugins/splash/ubuntu-text/Makefile
           src/plugins/splash/text/Makefile
           src/plugins/splash/details/Makefile
           src/plugins/splash/space-flares/Makefile
           src/plugins/splash/two-step/Makefile
           src/plugins/splash/script/Makefile
           src/plugins/controls/Makefile
           src/plugins/controls/label/Makefile
           src/Makefile
           src/client/ply-boot-client.pc
           src/client/Makefile
           src/upstart-bridge/Makefile
           themes/Makefile
           themes/spinfinity/Makefile
           themes/fade-in/Makefile
           themes/tribar/Makefile
           themes/text/Makefile
           themes/details/Makefile
           themes/solar/Makefile
           themes/glow/Makefile
           themes/spinner/Makefile
           themes/script/Makefile
           themes/ubuntu-logo/Makefile
           themes/bgrt/Makefile
           themes/ubuntu-text/Makefile
           images/Makefile
	   scripts/plymouth-generate-initrd
	   scripts/plymouth-populate-initrd
	   scripts/plymouth-set-default-theme
           scripts/Makefile
           systemd-units/plymouth-halt.service
           systemd-units/plymouth-kexec.service
           systemd-units/plymouth-poweroff.service
           systemd-units/plymouth-quit.service
           systemd-units/plymouth-quit-wait.service
           systemd-units/plymouth-read-write.service
           systemd-units/plymouth-reboot.service
           systemd-units/plymouth-start.service
           systemd-units/plymouth-switch-root.service
           systemd-units/plymouth-switch-root-initramfs.service
           systemd-units/systemd-ask-password-plymouth.path
           systemd-units/systemd-ask-password-plymouth.service
           systemd-units/Makefile
           docs/Makefile
])
AC_OUTPUT
