wms = \
	Enlightenment \
	IceWM \
	Scwm \
	WindowMaker \
	twm

desktop_files = $(wms:=.desktop)
desktop_in_files = $(desktop_files:.desktop=.desktop.in)
desktop_in_in_files = $(desktop_files:.desktop=.desktop.in.in)

wmdatadir = $(datadir)/gnome/wm-properties
wmdata_DATA = $(desktop_files)

EXTRA_DIST = $(desktop_in_in_files)

$(desktop_in_files): %.desktop.in: %.desktop.in.in
	    sed s#Icon=#Icon=$(GNOMECC_ICONS_DIR)/# < $< > $@

$(desktop_files): %.desktop: %.desktop.in
	$(top_builddir)/xml-i18n-merge -d $(top_srcdir)/po $< $@
