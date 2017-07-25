SUBDIRS= \
	common \
	background \
	power \
	color \
	display \
	mouse \
	notifications \
	online-accounts \
	region \
	info \
	sound \
	keyboard \
	universal-access \
	user-accounts \
	datetime \
	search \
	privacy \
	sharing \
	printers

if BUILD_WACOM
SUBDIRS += wacom
endif

if BUILD_NETWORK
SUBDIRS += network
endif

if BUILD_BLUETOOTH
SUBDIRS += bluetooth
endif

EXTRA_DIST = meson.build

-include $(top_srcdir)/git.mk
