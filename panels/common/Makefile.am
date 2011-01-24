noinst_LTLIBRARIES = liblanguage.la

AM_CPPFLAGS =						\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-DDATADIR=\""$(datadir)"\"			\
	-DUIDIR=\""$(pkgdatadir)/ui/user-accounts"\"	\
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\"       \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DUM_PIXMAP_DIR=\""$(pkgdatadir)/pixmaps"\"	\
	$(PANEL_CFLAGS)					\
	$(GNOME_DESKTOP_CFLAGS) 			\
	$(POLKIT_CFLAGS)				\
	$(CHEESE_CFLAGS)				\
	$(DBUS_CFLAGS)					\
	$(GIO_CFLAGS)					\
	$(WARN_CFLAGS)

liblanguage_la_CFLAGS = 		\
	$(PANEL_CFLAGS)

liblanguage_la_SOURCES =		\
	gdm-languages.h 		\
	gdm-languages.c 		\
	locarchive.h			\
	cc-common-language.c		\
	cc-common-language.h

liblanguage_la_LIBADD = 		\
	$(PANEL_LIBS)

liblanguage_la_LDFLAGS = $(PANEL_LDFLAGS)

-include $(top_srcdir)/git.mk
