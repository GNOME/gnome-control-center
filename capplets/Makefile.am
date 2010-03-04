NULL =

SUBDIRS = \
	accessibility		\
	appearance		\
	keyboard		\
	mouse			\
	network			\
	security		\
	$(NULL)

DIST_SUBDIRS = \
	accessibility		\
	appearance		\
	default-applications	\
	keyboard		\
	mouse			\
	network			\
	security		\
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
