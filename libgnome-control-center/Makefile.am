NULL =

uidir = $(pkgdatadir)/ui

AM_CPPFLAGS =					\
	$(LIBGNOME_CONTROL_CENTER_CFLAGS)	\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""\
	-DUIDIR="\"$(uidir)\""			\
	-DMENUDIR="\"$(menudir)\""		\
	$(NULL)

noinst_LTLIBRARIES = libgnome-control-center.la

libgnome_control_center_la_SOURCES =		\
	cc-panel.c				\
	cc-panel.h				\
	cc-shell.c				\
	cc-shell.h				\
	cc-editable-entry.c			\
	cc-editable-entry.h			\
	$(NULL)

libgnome_control_center_la_LDFLAGS =		\
	-no-undefined				\
	$(NULL)

libgnome_control_center_la_LIBADD = 		\
	$(LIBGNOME_CONTROL_CENTER_LIBS)		\
	$(NULL)

-include $(top_srcdir)/git.mk
