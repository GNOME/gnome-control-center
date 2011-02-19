# This is used in PANEL_CFLAGS
cappletname = common

noinst_LTLIBRARIES = liblanguage.la

AM_CPPFLAGS =						\
	$(PANEL_CFLAGS)					\
	$(LIBLANGUAGE_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"			\
	-DUIDIR=\""$(pkgdatadir)/ui/user-accounts"\"	\
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\"       \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DUM_PIXMAP_DIR=\""$(pkgdatadir)/pixmaps"\"

liblanguage_la_SOURCES =		\
	gdm-languages.h 		\
	gdm-languages.c 		\
	locarchive.h			\
	cc-common-language.c		\
	cc-common-language.h

liblanguage_la_LIBADD = 		\
	$(PANEL_LIBS)			\
	$(LIBLANGUAGE_LIBS)

liblanguage_la_LDFLAGS = $(PANEL_LDFLAGS)

-include $(top_srcdir)/git.mk
