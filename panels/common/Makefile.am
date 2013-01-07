# This is used in PANEL_CFLAGS
cappletname = common

noinst_LTLIBRARIES = liblanguage.la
noinst_PROGRAMS = list-languages

AM_CPPFLAGS =						\
	$(PANEL_CFLAGS)					\
	$(LIBLANGUAGE_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"			\
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\"       \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"

BUILT_SOURCES =			\
	cc-common-resources.c	\
	cc-common-resources.h

liblanguage_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	gdm-languages.h 		\
	gdm-languages.c 		\
	locarchive.h			\
	cc-common-language.c		\
	cc-common-language.h		\
	cc-language-chooser.c		\
	cc-language-chooser.h

liblanguage_la_LIBADD = 		\
	$(LIBLANGUAGE_LIBS)

list_languages_SOURCES = list-languages.c
list_languages_LDADD = liblanguage.la
list_languages_CFLAGS = $(LIBLANGUAGE_CFLAGS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/common.gresource.xml)
cc-common-resources.c: common.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_common $<
cc-common-resources.h: common.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_common $<

rulesdir = $(datadir)/polkit-1/rules.d/
rules_DATA = gnome-control-center.rules

EXTRA_DIST = $(rules_DATA) $(resource_files) common.gresource.xml
CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
