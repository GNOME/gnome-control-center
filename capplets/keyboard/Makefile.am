@XML_I18N_MERGE_DESKTOP_RULE@

cappletname=keyboard

Gladedir = $(datadir)/control-center-data
Glade_DATA = $(cappletname)-properties.glade

EXTRA_DIST = \
  $(Glade_DATA) \
  $(GNOMECC_CAPPLETS_EXTRA_DIST)

INCLUDES = \
     -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	@GNOMECC_CAPPLETS_CFLAGS@

bin_PROGRAMS = keyboard-properties

keyboard_properties_SOURCES = \
	prefs-widget.c prefs-widget.h \
	preferences.c  preferences.h \
	main.c

keyboard_properties_LDADD = @CAPPLET_LIBS@ $(XF86MISC_LIBS)

# desktop.in.in -> desktop.in -> desktop
#
desktop = $(cappletname)-properties.desktop

iconsdir = $(GNOMECC_ICONS_DIR)
icons_DATA = keyboard-capplet.png

$(desktop).in: %.desktop.in: %.desktop.in.in
	sed s#Icon=.\*#Icon=$(GNOMECC_ICONS_DIR)/$(cappletname)-capplet.png# < $< > $(cappletname)-properties.desktop.in

install-data-local:
	$(mkinstalldirs) $(datadir)/control-center/capplets/
	$(INSTALL_DATA) $(srcdir)/$(desktop) $(datadir)/control-center/capplets/$(desktop)

install-data-am: install-data-local

all-local: $(desktop)
