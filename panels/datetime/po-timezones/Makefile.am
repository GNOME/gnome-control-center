CATALOGS = $(patsubst %.po,%.mo,$(wildcard *.po))

all: $(CATALOGS)

NLSPACKAGE = $(GETTEXT_PACKAGE)-timezones

%.mo: %.po
	$(AM_V_GEN) msgfmt -o $@ $<

install: $(CATALOGS)
	@mkdir -p $(DESTDIR)$(localedir)
	for n in $(CATALOGS); do \
		l=`basename $$n .mo`; \
		$(INSTALL) -m 755 -d $(DESTDIR)$(localedir)/$$l; \
		$(INSTALL) -m 755 -d $(DESTDIR)$(localedir)/$$l/LC_MESSAGES; \
		if [ -f $$n ]; then \
			$(INSTALL) -m 644 $$n $(DESTDIR)$(localedir)/$$l/LC_MESSAGES/$(NLSPACKAGE).mo; \
		fi; \
	done

EXTRA_DIST=$(wildcard *.po)

CLEANFILES = $(CATALOGS)

-include $(top_srcdir)/git.mk
