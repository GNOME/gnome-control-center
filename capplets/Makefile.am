SUBDIRS = \
	common			\
	accessibility	\
	appearance		\
	default-applications	\
	keybindings		\
	keyboard		\
	mouse			\
	network			\
	windows

DIST_SUBDIRS = \
	common			\
	accessibility		\
	appearance		\
	default-applications	\
	keybindings		\
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
