INCLUDES=$(GNOME_SETTINGS_DAEMON_CFLAGS) -I$(top_srcdir)/libbackground -I$(top_srcdir) \
  -DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
  -DESD_SERVER="\"$(ESD_SERVER)\""	\
  -DDATADIR="\"$(datadir)\""
bin_PROGRAMS=gnome-settings-daemon

gnome_settings_daemon_SOURCES =	\
	factory.c			\
	gnome-settings-daemon.h		\
	gnome-settings-daemon.c		\
	gnome-settings-font.h		\
	gnome-settings-font.c		\
	gnome-settings-mouse.h		\
	gnome-settings-mouse.c		\
	gnome-settings-keyboard.h	\
	gnome-settings-keyboard.c	\
	gnome-settings-background.h	\
	gnome-settings-background.c	\
	gnome-settings-xsettings.c	\
	gnome-settings-xsettings.h	\
	gnome-settings-wm.c		\
	gnome-settings-wm.h		\
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
	xsettings-common.c		\
	xsettings-manager.c		\
	xsettings-common.h		\
	xsettings-manager.h		\
	gnome-settings-keybindings.c    \
	gnome-settings-keybindings.h    \
	$(CORBA_GENERATED)

#	$(AccessX_files)		

gnome_settings_daemon_LDADD = \
	$(GNOME_SETTINGS_DAEMON_LIBS)					\
	$(top_builddir)/libbackground/libbackground.la			\
	$(top_builddir)/libsounds/libsounds.a				\
	-L$(top_builddir)/libwindow-settings/ -lgnome-window-settings 	\
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

$(CORBA_GENERATED): $(idl_dir)/GNOME_SettingsDaemon.idl
	$(ORBIT_IDL) $(IDL_FLAGS) $(idl_dir)/GNOME_SettingsDaemon.idl

serverdir	= $(libdir)/bonobo/servers
server_in_files = GNOME_SettingsDaemon.server.in
server_DATA     = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST =						\
	$(BUILT_SOURCES)				\
	$(server_DATA) 					\
	$(server_in_files)				\
