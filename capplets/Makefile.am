always_built_SUBDIRS =		\
	common			\
	accessibility		\
	appearance		\
	default-applications	\
	keyboard		\
	mouse			\
	sound			\
	keybindings		\
	network			\
	windows

SUBDIRS = $(always_built_SUBDIRS)

if HAVE_RANDR
SUBDIRS += display
endif

if BUILD_ABOUTME
SUBDIRS += about-me
endif

DIST_SUBDIRS = \
	$(always_built_SUBDIRS) display about-me
