wmdatadir = $(datadir)/gnome/wm-properties

wmdata_in_files= \
	Enlightenment.desktop.in \
	IceWM.desktop.in \
	Scwm.desktop.in \
        WindowMaker.desktop.in \
	twm.desktop.in

wmdata_DATA = $(wmdata_in_files:.desktop.in=.desktop)
@XML_I18N_MERGE_DESKTOP_RULE@

EXTRA_DIST= $(wmdata_in_files)

