INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOME_INCLUDEDIR) \
	$(XML_CFLAGS) \
	-I$(top_srcdir)/

Screensaversdir = $(datadir)/screensavers
Screensavers_DATA = \
	qix.xml \
	attraction.xml

EXTRA_DIST = $(Screensavers_DATA)

noinst_PROGRAMS = extract-labels

extract_labels_SOURCES = extract-labels.c

extract_labels_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOME_LIBS) \
	$(GNOME_XML_LIB)

qix.xml.h: qix.xml extract-labels
	./extract-labels qix.xml
attraction.xml.h: attraction.xml extract-labels
	./extract-labels attraction.xml
