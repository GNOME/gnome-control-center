INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOME_INCLUDEDIR) \
	$(XML_CFLAGS) \
	-I$(top_srcdir)/

Screensaversdir = $(datadir)/screensavers
Screensavers_DATA = \
	ant.xml \
	bsod.xml \
	compass.xml \
	flag.xml \
	qix.xml \
	attraction.xml

EXTRA_DIST = $(Screensavers_DATA) README

noinst_PROGRAMS = extract-labels

extract_labels_SOURCES = extract-labels.c

extract_labels_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOME_LIBS) \
	$(GNOME_XML_LIB)

BUILT_SOURCES = ant.xml.h bsod.xml.h compass.xml.h flag.xml.h \
		qix.xml.h attraction.xml.h

ant.xml.h: ant.xml extract-labels
	./extract-labels ant.xml
bsod.xml.h: bsod.xml extract-labels
	./extract-labels bsod.xml
compass.xml.h: compass.xml extract-labels
	./extract-labels compass.xml
flag.xml.h: flag.xml extract-labels
	./extract-labels flag.xml
qix.xml.h: qix.xml extract-labels
	./extract-labels qix.xml
attraction.xml.h: attraction.xml extract-labels
	./extract-labels attraction.xml
