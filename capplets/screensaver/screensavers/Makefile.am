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

screensavers_msgs = \
	ant.xml.h \
	bsod.xml.h \
	compass.xml.h \
	flag.xml.h \
	qix.xml.h \
	attraction.xml.h

EXTRA_DIST = $(Screensavers_DATA) $(screensavers_msgs) README

noinst_PROGRAMS = extract-labels

extract_labels_SOURCES = extract-labels.c

extract_labels_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOME_LIBS) \
	$(GNOME_XML_LIB)

ant.xml.h: $(srcdir)/ant.xml extract-labels
	./extract-labels $(srcdir)/ant.xml
bsod.xml.h: $(srcdir)/bsod.xml extract-labels
	./extract-labels $(srcdir)/bsod.xml
compass.xml.h: $(srcdir)/compass.xml extract-labels
	./extract-labels $(srcdir)/compass.xml
flag.xml.h: $(srcdir)/flag.xml extract-labels
	./extract-labels $(srcdir)/flag.xml
qix.xml.h: $(srcdir)/qix.xml extract-labels
	./extract-labels $(srcdir)/qix.xml
attraction.xml.h: $(srcdir)/attraction.xml extract-labels
	./extract-labels $(srcdir)/attraction.xml
