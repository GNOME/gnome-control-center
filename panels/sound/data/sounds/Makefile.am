NULL =

sounddir = $(datadir)/sounds/gnome/default/alerts

sound_DATA =			\
	bark.ogg		\
	drip.ogg		\
	glass.ogg		\
	sonar.ogg		\
	$(NULL)

metadata_in_files = gnome-sounds-default.xml.in
metadatadir = $(pkgdatadir)/sounds
metadata_DATA = $(metadata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

noinst_DATA = gnome-sounds-default.xml.in
CLEANFILES = gnome-sounds-default.xml gnome-sounds-default.xml.in

EXTRA_DIST = $(sound_DATA) gnome-sounds-default.xml.in.in meson.build

gnome-sounds-default.xml.in: gnome-sounds-default.xml.in.in Makefile
	$(AM_V_GEN)sed -e 's^\@datadir\@^$(datadir)^g' < $(srcdir)/gnome-sounds-default.xml.in.in > gnome-sounds-default.xml.in.tmp \
	&& mv gnome-sounds-default.xml.in.tmp gnome-sounds-default.xml.in

MAINTAINERCLEANFILES = \
	Makefile.in

-include $(top_srcdir)/git.mk
