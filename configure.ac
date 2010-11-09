m4_define([gnome_control_center_version], 2.91.0)
AC_INIT([gnome-control-center], [gnome_control_center_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-control-center])

AC_CONFIG_SRCDIR([shell])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.10 no-dist-gzip dist-bzip2 tar-ustar])
AM_MAINTAINER_MODE([enable])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

# .so version for libgnome-control-center
LIBGNOMECONTROLCENTER_CURRENT=1
LIBGNOMECONTROLCENTER_REVISION=0
LIBGNOMECONTROLCENTER_AGE=0
AC_SUBST(LIBGNOMECONTROLCENTER_CURRENT)
AC_SUBST(LIBGNOMECONTROLCENTER_REVISION)
AC_SUBST(LIBGNOMECONTROLCENTER_AGE)

# Use the GNOME documentation framework
GNOME_DOC_INIT

# Internationalization support

IT_PROG_INTLTOOL([0.40.0])

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=gnome-control-center-2.0
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package])

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

AC_PATH_XTRA
x_libs="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"

PKG_CHECK_MODULES(LIBCANBERRA_GTK, libcanberra-gtk3, [
  CANBERRA_GTK=1
  AC_SUBST(LIBCANBERRA_GTK_CFLAGS)
  AC_SUBST(LIBCANBERRA_GTK_LIBS)
  ], [:])
AM_CONDITIONAL(HAVE_LIBCANBERRA_GTK, test "x$CANBERRA_GTK" = "x1")

dnl Region panel
savecppflags=$CPPFLAGS
CPPFLAGS="$CPPFLAGS $X_CFLAGS"
AC_CHECK_HEADERS([X11/Xlib.h])
AC_CHECK_LIB(Xxf86misc, XF86MiscQueryExtension, [
  AC_CHECK_HEADERS([X11/extensions/xf86misc.h], [XF86MISC_LIBS="-lXxf86misc"],[],
[#if HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#endif
])])
AC_SUBST(XF86MISC_LIBS)
AC_CHECK_HEADERS(X11/extensions/XKB.h)
CPPFLAGS=$savecppflags

AC_CHECK_LIB(m, floor)

dnl ==============================================
dnl Check that we meet the  dependencies
dnl ==============================================

GLIB_REQUIRED_VERSION=2.25.11
GTK_REQUIRED_VERSION=2.91.3
DESKTOP_SCHEMAS_REQUIRED_VERSION=0.0.2
PA_REQUIRED_VERSION=0.9.16
CANBERRA_REQUIRED_VERSION=0.13

COMMON_MODULES="gtk+-3.0 >= $GTK_REQUIRED_VERSION
 glib-2.0 >= $GLIB_REQUIRED_VERSION
 gthread-2.0
 gio-2.0
 gconf-2.0
 libxml-2.0
 gsettings-desktop-schemas >= $DESKTOP_SCHEMAS_REQUIRED_VERSION"
PKG_CHECK_MODULES(CAPPLET, $COMMON_MODULES)
PKG_CHECK_MODULES(GNOMECC_SHELL, $COMMON_MODULES libgnome-menu gio-unix-2.0)
PKG_CHECK_MODULES(DBUS, dbus-1 dbus-glib-1)
PKG_CHECK_MODULES(GNOME_DESKTOP, gnome-desktop-3.0)
PKG_CHECK_MODULES(DEFAULT_APPLICATIONS_CAPPLET, libxml-2.0)
PKG_CHECK_MODULES(GSD_DBUS, gnome-settings-daemon)
PKG_CHECK_MODULES(GIO, gio-2.0 gio-unix-2.0)
PKG_CHECK_MODULES(XML, libxml-2.0)
PKG_CHECK_MODULES(CANBERRA, libcanberra-gtk3 >= $CANBERRA_REQUIRED_VERSION)
AC_SUBST(CANBERRA_CFLAGS)
AC_SUBST(CANBERRA_LIBS)
PKG_CHECK_MODULES(PULSEAUDIO,
          libpulse >= $PA_REQUIRED_VERSION
          libpulse-mainloop-glib >= $PA_REQUIRED_VERSION)
AC_SUBST(PULSEAUDIO_CFLAGS)
AC_SUBST(PULSEAUDIO_LIBS)

gtk_lib_dir=`$PKG_CONFIG --variable libdir gtk+-3.0`
gtk_binary_version=`$PKG_CONFIG --variable gtk_binary_version gtk+-3.0`
GTK_ENGINE_DIR="$gtk_lib_dir/gtk-3.0/$gtk_binary_version/engines"
AC_SUBST(GTK_ENGINE_DIR)

PKG_CHECK_MODULES(GLIB, glib-2.0)

PKG_CHECK_MODULES(POLKIT, polkit-gobject-1 >= 0.97)
PKG_CHECK_MODULES(CHEESE, gstreamer-0.10 cheese-gtk >= 2.29.90, have_cheese=yes, have_cheese=no)

if test x$have_cheese = xyes ; then
        AC_DEFINE(HAVE_CHEESE, 1, [Define to 1 to enable cheese webcam support])
fi

AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
ISO_CODES=iso-codes

dnl
dnl Check for Xft version 2; we build in extra functionality to the font capplet
dnl when we have it.
dnl
xft_modules=
if $PKG_CONFIG --exists xft ; then
  xft_modules="xft"
  AC_DEFINE(HAVE_XFT2,,[Define if Xft functionality is available])
fi

PKG_CHECK_MODULES(FONT_CAPPLET, $COMMON_MODULES $xft_modules)

PKG_CHECK_MODULES(AT_CAPPLET, $COMMON_MODULES)

PKG_CHECK_MODULES(DISPLAY_CAPPLET, $COMMON_MODULES)
DISPLAY_CAPPLET_LIBS="$DISPLAY_CAPPLET_LIBS"

CAPPLET_LIBS="$CAPPLET_LIBS $x_libs"
GNOMECC_LIBS="$GNOMECC_LIBS $x_libs"

dnl =============================================
dnl X Input library >= 1.2 with property support
dnl =============================================
PKG_CHECK_MODULES(XINPUT, [xi >= 1.2])

CAPPLET_LIBS="$CAPPLET_LIBS $XINPUT_LIBS"

dnl ==============
dnl gswitchit
dnl ==============
PKG_CHECK_MODULES(LIBGNOMEKBD, [libgnomekbd >= 2.31.1 libxklavier >= 4.0])

PKG_CHECK_MODULES(LIBGNOMEKBDUI, [libgnomekbdui >= 2.31.2])

dnl ==============================================
dnl End: Check that we meet the  dependencies
dnl ==============================================

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal, no)

if test x"$GLIB_GENMARSHAL" = xno; then
  AC_MSG_ERROR([glib-genmarshal executable not found in your path - should be installed with glib])
fi

AC_SUBST(GLIB_GENMARSHAL)

dnl ==============================================
dnl Special GConf section
dnl ==============================================

AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test x"$GCONFTOOL" = xno; then
  AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

dnl ==============================================
dnl Define the main variables
dnl ==============================================
EXTRA_CFLAGS="-I\$(top_srcdir)/ -DG_LOG_DOMAIN=\"\\\"\$(cappletname)-properties\\\"\""

GNOMECC_CAPPLETS_CFLAGS="${CAPPLET_CFLAGS} ${EXTRA_CFLAGS} ${DBUS_CFLAGS}"
GNOMECC_CAPPLETS_LIBS="${CAPPLET_LIBS} ${DBUS_LIBS}"

GNOMECC_CAPPLETS_CLEANFILES="\$(desktop) \$(desktop).in"
GNOMECC_CAPPLETS_EXTRA_DIST="ChangeLog \$(desktop).in.in \$(cappletname)-capplet.png \$(pixmaps_DATA)"

AC_SUBST(GNOMECC_CAPPLETS_EXTRA_DIST)
AC_SUBST(GNOMECC_CAPPLETS_CLEANFILES)
AC_SUBST(GNOMECC_CAPPLETS_CFLAGS)
AC_SUBST(GNOMECC_CAPPLETS_LIBS)



dnl =======================================
dnl Panels
dnl =======================================

PANELS_DIR="${libdir}/control-center-1/panels"
AC_SUBST(PANELS_DIR)

PANEL_CFLAGS="-I\$(top_srcdir)/libgnome-control-center/"
AC_SUBST(PANEL_CFLAGS)

PANEL_LIBS="\$(top_builddir)/libgnome-control-center/libgnome-control-center.la"
AC_SUBST(PANEL_LIBS)

PANEL_LDFLAGS="-export_dynamic -avoid-version -module -no-undefined -export-symbols-regex '^g_io_module_(load|unload)'"
AC_SUBST(PANEL_LDFLAGS)

dnl ==============================================
dnl Example Panel
dnl ==============================================

AC_MSG_CHECKING([whether to build the example panel])
AC_ARG_ENABLE([examples],
   AS_HELP_STRING([--enable-examples],
                  [enable the examples]),,
   [enable_examples=no])
AC_MSG_RESULT([$enable_examples])

AM_CONDITIONAL(BUILD_EXAMPLES, test "x$enable_examples" = "xyes")

dnl ==============================================
dnl libsocialweb
dnl ==============================================

AC_MSG_CHECKING([Enable libsocialweb support])
AC_ARG_WITH([libsocialweb],
   AS_HELP_STRING([--with-libsocialweb],
                  [enable libsocialweb support]),,
   [with_libsocialweb=no])
AC_MSG_RESULT([$with_libsocialweb])

if test "x$with_libsocialweb" == "xyes"; then
  PKG_CHECK_MODULES(SOCIALWEB, libsocialweb-client)
  AC_DEFINE(HAVE_LIBSOCIALWEB, 1, [Defined if libsocialweb is available])
fi
AM_CONDITIONAL(WITH_LIBSOCIALWEB, test "x$with_libsocialweb" = "xyes")


dnl =======================================
dnl Update Mime Database
dnl =======================================

AC_PATH_PROG(UPDATE_MIME_DATABASE, update-mime-database, no)

AC_ARG_ENABLE(update-mimedb,
   AS_HELP_STRING([--disable-update-mimedb],
                  [do not update mime database after installation]),,
                   enable_update_mimedb=yes)
AM_CONDITIONAL(ENABLE_UPDATE_MIMEDB, test x$enable_update_mimedb = xyes)

# check for gtk-doc
GTK_DOC_CHECK([1.9])

CONTROL_CENTER_VERSION=gnome_control_center_version
AC_SUBST(CONTROL_CENTER_VERSION)

dnl =======================================
dnl Finish
dnl =======================================

# Turn on the additional warnings last

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
        CFLAGS="\
        -Wall -Wclobbered -Wempty-body -Wignored-qualifiers \
        -Wmissing-field-initializers -Wmissing-parameter-type \
        -Wold-style-declaration -Woverride-init -Wtype-limits \
        -Wuninitialized \
        -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
        -Wnested-externs -Wpointer-arith \
        -Wcast-align -Wsign-compare -Wp,-D_FORTIFY_SOURCE=2 \
        $CFLAGS"

        for option in -Wno-strict-aliasing -Wno-sign-compare; do
                SAVE_CFLAGS="$CFLAGS"
                CFLAGS="$CFLAGS $option"
                AC_MSG_CHECKING([whether gcc understands $option])
                AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
                                  [has_option=yes],
                                  [has_option=no])
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


AC_CONFIG_FILES([
Makefile
docs/Makefile
docs/reference/Makefile
docs/reference/libgnome-control-center/Makefile
docs/reference/libgnome-control-center/version.xml
examples/Makefile
examples/gnome-example-panel.desktop.in
help/Makefile
libgnome-control-center/Makefile
libgnome-control-center/libgnome-control-center.pc
panels/Makefile
panels/background/Makefile
panels/background/gnome-background-panel.desktop.in
panels/datetime/Makefile
panels/datetime/gnome-datetime-panel.desktop.in
panels/default-applications/Makefile
panels/default-applications/gnome-at-commandline.in
panels/default-applications/gnome-at-session.desktop.in
panels/default-applications/gnome-default-applications-panel.desktop.in
panels/default-applications/gnome-default-applications.pc
panels/display/Makefile
panels/display/gnome-display-panel.desktop.in
panels/keyboard/Makefile
panels/keyboard/gnome-keyboard-panel.desktop.in
panels/keyboard/gnome-keybindings.pc
panels/region/Makefile
panels/region/gnome-region-panel.desktop.in
panels/mouse/Makefile
panels/mouse/gnome-mouse-panel.desktop.in
panels/network/Makefile
panels/network/gnome-network-panel.desktop.in
panels/sound/Makefile
panels/sound/data/Makefile
panels/sound/data/gnome-sound-panel.desktop.in
panels/sound/data/symbolic-icons/Makefile
panels/sound/data/symbolic-icons/scalable/Makefile
panels/sound/data/symbolic-icons/scalable/status/Makefile
panels/sound/data/icons/Makefile
panels/sound/data/icons/16x16/Makefile
panels/sound/data/icons/16x16/apps/Makefile
panels/sound/data/icons/16x16/status/Makefile
panels/sound/data/icons/22x22/Makefile
panels/sound/data/icons/22x22/apps/Makefile
panels/sound/data/icons/22x22/status/Makefile
panels/sound/data/icons/24x24/Makefile
panels/sound/data/icons/24x24/apps/Makefile
panels/sound/data/icons/24x24/status/Makefile
panels/sound/data/icons/32x32/Makefile
panels/sound/data/icons/32x32/apps/Makefile
panels/sound/data/icons/32x32/status/Makefile
panels/sound/data/icons/48x48/Makefile
panels/sound/data/icons/48x48/apps/Makefile
panels/sound/data/icons/scalable/Makefile
panels/sound/data/icons/scalable/apps/Makefile
panels/sound/data/icons/scalable/devices/Makefile
panels/sound/data/sounds/Makefile
panels/universal-access/Makefile
panels/universal-access/gnome-universal-access-panel.desktop.in
panels/user-accounts/Makefile
panels/user-accounts/data/Makefile
panels/user-accounts/data/gnome-user-accounts-panel.desktop.in
panels/user-accounts/data/icons/Makefile
po/Makefile.in
shell/Makefile
shell/gnome-control-center.desktop.in
])

dnl due to a bug in intltool we need to expand something from the root last control-center.spec
AC_OUTPUT
