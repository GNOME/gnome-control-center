SUBDIRS= \
	common \
	background \
	screen \
	power \
	display \
	media \
	mouse \
	region \
	info \
	sound \
	keyboard \
	universal-access \
	user-accounts \
	datetime

if BUILD_PRINTERS
SUBDIRS += printers
endif

if BUILD_NETWORK
SUBDIRS += network
endif

-include $(top_srcdir)/git.mk
