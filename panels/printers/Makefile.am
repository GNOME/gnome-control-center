include $(top_srcdir)/Makefile.decl

cappletname = printers

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(PRINTERS_PANEL_CFLAGS)			\
	$(CUPS_CFLAGS)					\
	$(SMBCLIENT_CFLAGS)				\
	-I$(top_srcdir)/shell/				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DTEST_SRCDIR=\""$(srcdir)/"\"			\
	$(NULL)

noinst_LTLIBRARIES = libprinters.la

BUILT_SOURCES =			\
	cc-printers-resources.c	\
	cc-printers-resources.h

libprinters_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	pp-printer.c			\
	pp-printer.h			\
	pp-new-printer.c		\
	pp-new-printer.h		\
	pp-maintenance-command.c	\
	pp-maintenance-command.h	\
	pp-host.c			\
	pp-host.h			\
	pp-cups.c			\
	pp-cups.h			\
	pp-utils.c			\
	pp-utils.h			\
	pp-ppd-option-widget.c		\
	pp-ppd-option-widget.h		\
	pp-ipp-option-widget.c		\
	pp-ipp-option-widget.h		\
	pp-new-printer-dialog.c		\
	pp-new-printer-dialog.h		\
	pp-ppd-selection-dialog.c	\
	pp-ppd-selection-dialog.h	\
	pp-options-dialog.c		\
	pp-options-dialog.h		\
	pp-job.c			\
	pp-job.h			\
	pp-jobs-dialog.c		\
	pp-jobs-dialog.h		\
	pp-samba.c			\
	pp-samba.h			\
	pp-print-device.c		\
	pp-print-device.h		\
	cc-printers-panel.c		\
	cc-printers-panel.h

libprinters_la_CPPFLAGS = $(AM_CPPFLAGS)
libprinters_la_LIBADD = $(PRINTERS_PANEL_LIBS) $(PANEL_LIBS) $(CUPS_LIBS) $(SMBCLIENT_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/printers.gresource.xml)
cc-printers-resources.c: printers.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_printers $<
cc-printers-resources.h: printers.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_printers $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-printers-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) printers.gresource.xml

noinst_PROGRAMS = $(TEST_PROGS)
TEST_PROGS += test-shift test-canonicalization
test_shift_SOURCES = pp-print-device.c pp-print-device.h pp-utils.c pp-utils.h test-shift.c
test_shift_LDADD = $(PANEL_LIBS) $(PRINTERS_PANEL_LIBS) $(CUPS_LIBS)
test_canonicalization_SOURCES = pp-print-device.c pp-print-device.h pp-utils.c pp-utils.h test-canonicalization.c
test_canonicalization_LDADD = $(PANEL_LIBS) $(PRINTERS_PANEL_LIBS) $(CUPS_LIBS)

EXTRA_DIST +=				\
	shift-test.txt			\
	canonicalization-test.txt

-include $(top_srcdir)/git.mk
