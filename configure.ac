m4_define([gnome_control_center_version], 2.91.7)
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

IT_PROG_INTLTOOL([0.40.1])

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

AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

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
GTK_REQUIRED_VERSION=2.91.6
DESKTOP_SCHEMAS_REQUIRED_VERSION=0.1.7
PA_REQUIRED_VERSION=0.9.16
CANBERRA_REQUIRED_VERSION=0.13
GDKPIXBUF_REQUIRED_VERSION=2.23.0
POLKIT_REQUIRED_VERSION=0.97
GSD_REQUIRED_VERSION=2.91.9
NETWORK_MANAGER_REQUIRED_VERSION=0.8.992

COMMON_MODULES="gtk+-3.0 >= $GTK_REQUIRED_VERSION
 glib-2.0 >= $GLIB_REQUIRED_VERSION
 gthread-2.0
 gio-2.0
 gio-unix-2.0
 gsettings-desktop-schemas >= $DESKTOP_SCHEMAS_REQUIRED_VERSION"

PKG_CHECK_MODULES(LIBGNOME_CONTROL_CENTER, $COMMON_MODULES gconf-2.0)
PKG_CHECK_MODULES(LIBLANGUAGE, $COMMON_MODULES gnome-desktop-3.0)
PKG_CHECK_MODULES(SHELL, $COMMON_MODULES libgnome-menu gio-unix-2.0)
PKG_CHECK_MODULES(BACKGROUND_PANEL, $COMMON_MODULES libxml-2.0 gnome-desktop-3.0
                  gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED_VERSION)
PKG_CHECK_MODULES(DATETIME_PANEL, $COMMON_MODULES dbus-glib-1
                  polkit-gobject-1 >= $POLKIT_REQUIRED_VERSION
                  gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED_VERSION)
PKG_CHECK_MODULES(DISPLAY_PANEL, $COMMON_MODULES dbus-glib-1 gnome-desktop-3.0)
PKG_CHECK_MODULES(INFO_PANEL, $COMMON_MODULES libgtop-2.0)
PKG_CHECK_MODULES(KEYBOARD_PANEL, $COMMON_MODULES gconf-2.0)
PKG_CHECK_MODULES(MEDIA_PANEL, $COMMON_MODULES)
PKG_CHECK_MODULES(MOUSE_PANEL, $COMMON_MODULES xi >= 1.2
                  gnome-settings-daemon >= $GSD_REQUIRED_VERSION)
PKG_CHECK_MODULES(NETWORK_PANEL, $COMMON_MODULES)
PKG_CHECK_MODULES(POWER_PANEL, $COMMON_MODULES upower-glib >= 0.9.1)
PKG_CHECK_MODULES(PRINTERS_PANEL, $COMMON_MODULES dbus-glib-1
                  polkit-gobject-1 >= $POLKIT_REQUIRED_VERSION)
PKG_CHECK_MODULES(REGION_PANEL, $COMMON_MODULES libgnomekbd >= 2.91.1
                  libxklavier >= 5.1 libgnomekbdui >= 2.91.2)
PKG_CHECK_MODULES(SCREEN_PANEL, $COMMON_MODULES)
PKG_CHECK_MODULES(SOUND_PANEL, $COMMON_MODULES libxml-2.0
                  libcanberra-gtk3 >= $CANBERRA_REQUIRED_VERSION
                  libpulse >= $PA_REQUIRED_VERSION
                  libpulse-mainloop-glib >= $PA_REQUIRED_VERSION
                  gconf-2.0)
PKG_CHECK_MODULES(UNIVERSAL_ACCESS_PANEL, $COMMON_MODULES gconf-2.0)
PKG_CHECK_MODULES(USER_ACCOUNTS_PANEL, $COMMON_MODULES dbus-glib-1
                  polkit-gobject-1 >= $POLKIT_REQUIRED_VERSION
                  gnome-desktop-3.0
                  gdk-pixbuf-2.0 >= $GDKPIXBUF_REQUIRED_VERSION)

# Check for NetworkManager ~0.9
PKG_CHECK_MODULES(NETWORK_MANAGER, libnm-glib >= $NETWORK_MANAGER_REQUIRED_VERSION,
		  [have_networkmanager=yes], have_networkmanager=no)
if test "x$have_networkmanager" = xno ; then
        AC_MSG_WARN(*** Network panel will not be built (NetworkManager ~0.9 or newer not found) ***)
fi
AM_CONDITIONAL(BUILD_NETWORK, [test x$have_networkmanager = xyes])

# Check for CUPS 1.4 or newer
AC_PATH_PROG(CUPS_CONFIG, cups-config, no)
if test "x$CUPS_CONFIG" != "xno"; then
  CUPS_API_VERSION=`$CUPS_CONFIG --api-version`
  CUPS_API_MAJOR=`echo $ECHO_N $CUPS_API_VERSION | sed -e "s/\./\n/g" | sed -n "1p"`
  CUPS_API_MINOR=`echo $ECHO_N $CUPS_API_VERSION | sed -e "s/\./\n/g" | sed -n "2p"`

  AC_CHECK_HEADERS([cups/cups.h cups/http.h cups/ipp.h], have_cups_headers=yes, have_cups_headers=no)
  if test x$have_cups_headers = xyes ; then
    if test $CUPS_API_MAJOR -gt 1 -o \
            $CUPS_API_MAJOR -eq 1 -a $CUPS_API_MINOR -ge 4; then
      have_cups=yes
      CUPS_LIBS=-lcups
      AC_SUBST(CUPS_LIBS)
    else
      have_cups=no
      AC_MSG_WARN(*** Printers panel will not be built (CUPS 1.4 or newer not found) ***)
    fi
  else
    have_cups=no
    AC_MSG_WARN(*** Printers panel will not be built (CUPS header files not found) ***)
  fi
else
  have_cups=no
  AC_MSG_WARN(*** Printers panel will not be built (cups-config not found) ***)
fi
AM_CONDITIONAL(BUILD_PRINTERS, [test x$have_cups = xyes])

# Optional dependency for the user accounts panel
PKG_CHECK_MODULES(CHEESE, gstreamer-0.10 cheese-gtk >= 2.29.90, have_cheese=yes, have_cheese=no)

if test x$have_cheese = xyes ; then
        AC_DEFINE(HAVE_CHEESE, 1, [Define to 1 to enable cheese webcam support])
fi

# This is a hard-dependency for the region and user-accounts panels
PKG_CHECK_MODULES(ISOCODES, iso-codes)

AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
ISO_CODES=iso-codes

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

dnl =======================================
dnl Panels
dnl =======================================

PANELS_DIR="${libdir}/control-center-1/panels"
AC_SUBST(PANELS_DIR)

PANEL_CFLAGS="-I\$(top_srcdir)/libgnome-control-center/ -DG_LOG_DOMAIN=\"\\\"\$(cappletname)-cc-panel\\\"\""
AC_SUBST(PANEL_CFLAGS)

PANEL_LIBS="\$(top_builddir)/libgnome-control-center/libgnome-control-center.la"
AC_SUBST(PANEL_LIBS)

PANEL_LDFLAGS="-export_dynamic -avoid-version -module -no-undefined -export-symbols-regex '^g_io_module_(load|unload)'"
AC_SUBST(PANEL_LDFLAGS)

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
help/Makefile
libgnome-control-center/Makefile
libgnome-control-center/libgnome-control-center.pc
panels/Makefile
panels/common/Makefile
panels/background/Makefile
panels/background/gnome-background-panel.desktop.in
panels/datetime/Makefile
panels/datetime/gnome-datetime-panel.desktop.in
panels/datetime/po-timezones/Makefile
panels/display/Makefile
panels/display/gnome-display-panel.desktop.in
panels/keyboard/Makefile
panels/keyboard/gnome-keyboard-panel.desktop.in
panels/keyboard/gnome-keybindings.pc
panels/region/Makefile
panels/region/gnome-region-panel.desktop.in
panels/media/Makefile
panels/media/gnome-media-panel.desktop.in
panels/mouse/Makefile
panels/mouse/gnome-mouse-panel.desktop.in
panels/sound/Makefile
panels/sound/data/Makefile
panels/sound/data/gnome-sound-panel.desktop.in
panels/sound/data/symbolic-icons/Makefile
panels/sound/data/symbolic-icons/scalable/Makefile
panels/sound/data/symbolic-icons/scalable/status/Makefile
panels/sound/data/icons/Makefile
panels/sound/data/icons/16x16/Makefile
panels/sound/data/icons/16x16/apps/Makefile
panels/sound/data/icons/16x16/devices/Makefile
panels/sound/data/icons/16x16/status/Makefile
panels/sound/data/icons/22x22/Makefile
panels/sound/data/icons/22x22/apps/Makefile
panels/sound/data/icons/22x22/status/Makefile
panels/sound/data/icons/24x24/Makefile
panels/sound/data/icons/24x24/apps/Makefile
panels/sound/data/icons/24x24/devices/Makefile
panels/sound/data/icons/24x24/status/Makefile
panels/sound/data/icons/32x32/Makefile
panels/sound/data/icons/32x32/apps/Makefile
panels/sound/data/icons/32x32/devices/Makefile
panels/sound/data/icons/32x32/status/Makefile
panels/sound/data/icons/48x48/Makefile
panels/sound/data/icons/48x48/apps/Makefile
panels/sound/data/icons/48x48/devices/Makefile
panels/sound/data/icons/scalable/Makefile
panels/sound/data/icons/scalable/apps/Makefile
panels/sound/data/icons/scalable/devices/Makefile
panels/sound/data/sounds/Makefile
panels/screen/Makefile
panels/screen/gnome-screen-panel.desktop.in
panels/info/Makefile
panels/info/gnome-info-panel.desktop.in
panels/power/Makefile
panels/power/gnome-power-panel.desktop.in
panels/printers/Makefile
panels/printers/gnome-printers-panel.desktop.in
panels/network/Makefile
panels/network/gnome-network-panel.desktop.in
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
