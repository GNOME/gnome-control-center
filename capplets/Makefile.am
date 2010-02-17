NULL =

SUBDIRS = \
	appearance		\
	keyboard		\
	mouse			\
	network			\
	$(NULL)

DIST_SUBDIRS = \
	accessibility		\
	appearance		\
	default-applications	\
	keyboard		\
	mouse			\
	network			\
	windows			\
	display 		\
	about-me


if HAVE_RANDR
SUBDIRS += display
endif

if BUILD_ABOUTME
SUBDIRS += about-me
endif

-include $(top_srcdir)/git.mk
