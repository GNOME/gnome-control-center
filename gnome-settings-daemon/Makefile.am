SUBDIRS = xrdb

INCLUDES=$(LIBXKLAVIER_CFLAGS) $(GNOME_SETTINGS_DAEMON_CFLAGS) -I$(top_srcdir)/libbackground -I$(top_srcdir) \
  -DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
  -DESD_SERVER="\"$(ESD_SERVER)\""	\
  -DDATADIR="\"$(datadir)\""
libexec_PROGRAMS=gnome-settings-daemon

gnome_settings_daemon_SOURCES =	\
	factory.c			\
	gnome-settings-daemon.h		\
	gnome-settings-daemon.c		\
	gnome-settings-font.h		\
	gnome-settings-font.c		\
	gnome-settings-mouse.h		\
	gnome-settings-mouse.c		\
	gnome-settings-keyboard-xkb.h	\
	gnome-settings-keyboard-xkb.c	\
	gnome-settings-keyboard.h	\
	gnome-settings-keyboard.c	\
	gnome-settings-background.h	\
	gnome-settings-background.c	\
	gnome-settings-xsettings.c	\
	gnome-settings-xsettings.h	\
	gnome-settings-locate-pointer.c	\
	gnome-settings-locate-pointer.h	\
	gnome-settings-sound.c		\
	gnome-settings-sound.h		\
	gnome-settings-accessibility-keyboard.h	\
	gnome-settings-accessibility-keyboard.c	\
	gnome-settings-screensaver.h	\
	gnome-settings-screensaver.c	\
	gnome-settings-default-editor.c	\
	gnome-settings-default-editor.h	\
	gnome-settings-gtk1theme.c      \
	gnome-settings-gtk1theme.h	\
	gnome-settings-xrdb.c		\
	gnome-settings-xrdb.h		\
	xsettings-common.c		\
	xsettings-manager.c		\
	xsettings-common.h		\
	xsettings-manager.h		\
	gnome-settings-keybindings.c    \
	gnome-settings-keybindings.h    \
	gnome-settings-typing-break.c	\
	gnome-settings-typing-break.h	\
	reaper.c			\
	reaper.h			\
	$(CORBA_GENERATED)

#	$(AccessX_files)		

gnome_settings_daemon_LDADD = \
	$(LIBXKLAVIER_LIBS)					\
	$(GNOME_SETTINGS_DAEMON_LIBS)					\
	$(top_builddir)/libbackground/libbackground.la			\
	$(top_builddir)/libsounds/libsounds.a				\
	$(top_builddir)/libgswitchit/libgswitchit.a			\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la 	\
	$(XF86MISC_LIBS)

#AccessX_files = \
#	AccessXcomm.c		\
#	extutil.h		\
#	AccessXproto.h		\
#	AccessXlibint.h

CORBA_GENERATED_HEADER_FILES = GNOME_SettingsDaemon.h

CORBA_GENERATED = \
        $(CORBA_GENERATED_HEADER_FILES) \
        GNOME_SettingsDaemon-common.c \
        GNOME_SettingsDaemon-stubs.c \
        GNOME_SettingsDaemon-skels.c

BUILT_SOURCES=$(CORBA_GENERATED)

idl_dir = $(top_srcdir)/idl

IDL_FLAGS = -D__GNOME_SettingsDaemon_COMPILATION -I $(idl_dir) -I $(datadir)/idl \
        -I $(LIBBONOBO_IDL_DIR)                                 \
        -I $(BONOBO_ACTIVATION_IDL_DIR)

$(CORBA_GENERATED): $(idl_dir)/GNOME_SettingsDaemon.idl $(ORBIT_IDL)
	$(ORBIT_IDL) $(IDL_FLAGS) $(idl_dir)/GNOME_SettingsDaemon.idl

serverfile = GNOME_SettingsDaemon.server.in
serverdir = $(libdir)/bonobo/servers
install-data-hook :
	$(mkinstalldirs) $(serverdir)
	sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $(srcdir)/$(serverfile) > $(serverdir)/$(serverfile:.in=)

EXTRA_DIST = $(serverfile)
CLEANFILES = $(BUILT_SOURCES)
