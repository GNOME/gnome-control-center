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
	attraction.xml \
	rorschach.xml

screensavers_msgs = \
	ant.xml.h \
	bsod.xml.h \
	compass.xml.h \
	flag.xml.h \
	qix.xml.h \
	attraction.xml.h \
	rorschach.xml.h

EXTRA_DIST = $(Screensavers_DATA) $(screensavers_msgs) README

noinst_PROGRAMS = extract-labels

extract_labels_SOURCES = extract-labels.c

extract_labels_LDADD = \
	$(GNOME_LIBDIR) \
	$(GNOME_LIBS) \
	$(GNOME_XML_LIB)

ant.xml.h: $(srcdir)/ant.xml extract-labels
bsod.xml.h: $(srcdir)/bsod.xml extract-labels
compass.xml.h: $(srcdir)/compass.xml extract-labels
flag.xml.h: $(srcdir)/flag.xml extract-labels
qix.xml.h: $(srcdir)/qix.xml extract-labels
attraction.xml.h: $(srcdir)/attraction.xml extract-labels
rorschach.xml.h: $(srcdir)/rorschach.xml extract-labels

%.xml.h: %.xml
	./extract-labels $<
