@XML_I18N_MERGE_DESKTOP_RULE@

cappletname=keyboard

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	keyboard-properties.glade

iconsdir = $(GNOME_CONTROL_CENTER_ICONS_DIR)
icons_DATA = keyboard-capplet.png

EXTRA_DIST = \
  ChangeLog \
  $(Glade_DATA) \
  $(cappletname)-properties.desktop \
  $(cappletname)-properties.desktop.in \
  $(cappletname)-properties.desktop.in.in 

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"keyboard-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	@CAPPLET_CFLAGS@				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				

bin_PROGRAMS = keyboard-properties

keyboard_properties_SOURCES = \
	prefs-widget.c prefs-widget.h \
	preferences.c preferences.h \
	main.c

keyboard_properties_LDADD = @CAPPLET_LIBS@ $(XF86MISC_LIBS)

# desktop.in.in -> desktop.in -> desktop
#
desktop = $(cappletname)-properties.desktop

$(desktop).in: %.desktop.in: %.desktop.in.in
	sed s#Icon=.\*#Icon=$(GNOME_CONTROL_CENTER_ICONS_DIR)/$(cappletname)-capplet.png# < $< > $(cappletname)-properties.desktop.in

install-data-local:
	$(mkinstalldirs) /home/chema/foo
	$(INSTALL_DATA) $(srcdir)/$(desktop) $(datadir)/control-center/capplets/$(desktop)

install-data-am: install-data-local

all-local: $(desktop)
