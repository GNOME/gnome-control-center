SUBDIRS= \
	common \
	background \
	screen \
	power \
	color \
	display \
	mouse \
	online-accounts \
	region \
	info \
	sound \
	keyboard \
	universal-access \
	user-accounts \
	datetime \
	wacom

if BUILD_PRINTERS
SUBDIRS += printers
endif

if BUILD_NETWORK
SUBDIRS += network
endif

if BUILD_BLUETOOTH
SUBDIRS += bluetooth
endif

-include $(top_srcdir)/git.mk
